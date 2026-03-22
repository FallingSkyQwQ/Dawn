#include "dawn/core/download/download_service.h"

#include "dawn/core/pipeline/task_pipeline.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"
#include "dawn/infra/net/http_client_factory.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <future>
#include <optional>
#include <sstream>
#include <thread>
#include <mutex>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::net::HttpMethod;
using dawn::infra::net::HttpRequest;

std::string timestamped_id(const std::string& prefix, const std::string& seed) {
    std::string id = prefix;
    for (char ch : seed) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            id.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!id.empty() && id.back() != '-') {
            id.push_back('-');
        }
    }
    if (id == prefix) {
        id += "download";
    }
    id.push_back('-');
    id += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return id;
}

std::string make_download_error(const std::string& context, int attempt, int maxAttempts, const std::string& detail) {
    return context + " [attempt " + std::to_string(attempt) + "/" + std::to_string(maxAttempts) + "]: " + detail;
}

TaskPlan make_download_plan(const DownloadRequest& request) {
    TaskPlan plan;
    plan.id = request.id.empty() ? timestamped_id("download-", request.title.empty() ? request.url : request.title) : request.id;
    plan.title = request.title.empty() ? "Download" : request.title;
    plan.steps = {
        {"prepare", "Prepare destination", TaskStatus::Pending, 0, {}},
        {"download", "Download artifact", TaskStatus::Pending, 0, {}},
        {"verify", "Verify checksum", TaskStatus::Pending, 0, {}},
        {"finalize", "Finalize download", TaskStatus::Pending, 0, {}},
    };
    return plan;
}

HttpRequest make_get_request(const std::string& url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = url;
    request.headers.emplace("Accept", "*/*");
    return request;
}

std::vector<std::string> build_candidate_urls(const DownloadRequest& request) {
    std::vector<std::string> urls;
    if (!request.url.empty()) {
        urls.push_back(request.url);
    }
    for (const auto& mirror : request.mirrors) {
        if (!mirror.empty() && std::find(urls.begin(), urls.end(), mirror) == urls.end()) {
            urls.push_back(mirror);
        }
    }
    return urls;
}

std::optional<std::string> header_value_impl(const dawn::infra::net::HttpResponse& response, const std::string& headerName) {
    const auto to_lower = [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    };
    std::string wanted = headerName;
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), to_lower);
    for (const auto& [name, value] : response.headers) {
        std::string current = name;
        std::transform(current.begin(), current.end(), current.begin(), to_lower);
        if (current == wanted) {
            return value;
        }
    }
    return std::nullopt;
}

std::optional<std::uintmax_t> parse_total_size_from_content_range_impl(const std::string& contentRange) {
    const auto slash = contentRange.find('/');
    if (slash == std::string::npos || slash + 1 >= contentRange.size()) {
        return std::nullopt;
    }
    const auto totalText = contentRange.substr(slash + 1);
    if (totalText == "*") {
        return std::nullopt;
    }
    try {
        return static_cast<std::uintmax_t>(std::stoull(totalText));
    } catch (...) {
        return std::nullopt;
    }
}

} // namespace

DownloadService::DownloadService()
    : DownloadService(dawn::infra::net::HttpClientFactory::create_default_http_client(), 4) {
}

DownloadService::DownloadService(std::shared_ptr<dawn::infra::net::HttpClient> client)
    : DownloadService(std::move(client), 4) {
}

DownloadService::DownloadService(int maxConcurrency)
    : DownloadService(dawn::infra::net::HttpClientFactory::create_default_http_client(), maxConcurrency) {
}

DownloadService::DownloadService(std::shared_ptr<dawn::infra::net::HttpClient> client, int maxConcurrency)
    : client_(client ? std::move(client) : dawn::infra::net::HttpClientFactory::create_default_http_client())
    , maxConcurrency_(std::max(1, maxConcurrency)) {
    sleeper_ = [](std::chrono::milliseconds duration) {
        if (duration.count() > 0) {
            std::this_thread::sleep_for(duration);
        }
    };
}

std::string DownloadService::make_download_id() {
    return timestamped_id("download-", "job");
}

std::string DownloadService::make_plan_id(const std::string& title) {
    return timestamped_id("plan-", title);
}

std::string DownloadService::make_step_detail(const std::string& message, int attempt, int maxAttempts) {
    return message + " [attempt " + std::to_string(attempt) + "/" + std::to_string(maxAttempts) + "]";
}

std::vector<DownloadChunk> DownloadService::build_chunk_plan(std::uintmax_t totalSize, std::size_t chunkSize, std::uintmax_t startOffset) {
    std::vector<DownloadChunk> chunks;
    if (chunkSize == 0 || startOffset >= totalSize) {
        return chunks;
    }

    std::size_t index = 0;
    for (std::uintmax_t start = startOffset; start < totalSize; start += chunkSize, ++index) {
        const auto end = std::min<std::uintmax_t>(totalSize - 1, start + static_cast<std::uintmax_t>(chunkSize) - 1);
        chunks.push_back(DownloadChunk{index, start, end});
    }
    return chunks;
}

std::optional<std::uintmax_t> DownloadService::parse_total_size_from_content_range(const std::string& contentRange) {
    return ::dawn::core::parse_total_size_from_content_range_impl(contentRange);
}

std::optional<std::string> DownloadService::find_header_value(const dawn::infra::net::HttpResponse& response, const std::string& headerName) {
    return ::dawn::core::header_value_impl(response, headerName);
}

std::chrono::milliseconds DownloadService::throttle_duration(std::size_t bytes, std::size_t bytesPerSecond) {
    if (bytes == 0 || bytesPerSecond == 0) {
        return std::chrono::milliseconds{0};
    }
    const auto millis = static_cast<std::uint64_t>((bytes * 1000ULL + bytesPerSecond - 1ULL) / bytesPerSecond);
    return std::chrono::milliseconds{static_cast<std::chrono::milliseconds::rep>(std::max<std::uint64_t>(1ULL, millis))};
}

std::vector<std::string> DownloadService::candidate_urls(const DownloadRequest& request) const {
    return build_candidate_urls(request);
}

void DownloadService::set_max_concurrency(int maxConcurrency) {
    maxConcurrency_ = std::max(1, maxConcurrency);
}

int DownloadService::max_concurrency() const noexcept {
    return maxConcurrency_;
}

void DownloadService::set_bytes_per_second(std::size_t bytesPerSecond) {
    bytesPerSecond_ = bytesPerSecond;
}

std::size_t DownloadService::bytes_per_second() const noexcept {
    return bytesPerSecond_;
}

void DownloadService::set_sleeper(Sleeper sleeper) {
    sleeper_ = sleeper ? std::move(sleeper) : Sleeper{};
}

std::string DownloadService::enqueue(DownloadJob job) {
    if (job.id.empty()) {
        job.id = make_download_id();
    }
    jobs_.push_back(std::move(job));
    return jobs_.back().id;
}

std::vector<DownloadJob> DownloadService::jobs() const {
    return jobs_;
}

bool DownloadService::pause(const std::string& id) {
    const auto it = std::find_if(jobs_.begin(), jobs_.end(), [&](const DownloadJob& job) {
        return job.id == id;
    });
    if (it == jobs_.end()) {
        return false;
    }
    it->state = DownloadState::Paused;
    return true;
}

bool DownloadService::resume(const std::string& id) {
    const auto it = std::find_if(jobs_.begin(), jobs_.end(), [&](const DownloadJob& job) {
        return job.id == id;
    });
    if (it == jobs_.end()) {
        return false;
    }
    it->state = DownloadState::Running;
    return true;
}

bool DownloadService::mark_complete(const std::string& id) {
    const auto it = std::find_if(jobs_.begin(), jobs_.end(), [&](const DownloadJob& job) {
        return job.id == id;
    });
    if (it == jobs_.end()) {
        return false;
    }
    it->state = DownloadState::Completed;
    return true;
}

bool DownloadService::write_download_segment(const std::filesystem::path& destination, const std::string& data, bool append, std::string* error) const {
    if (append) {
        return dawn::infra::fs::append_binary_file(destination, data, error);
    }
    return dawn::infra::fs::write_binary_file(destination, data, error);
}

void DownloadService::throttle(std::size_t bytes) const {
    if (bytesPerSecond_ == 0 || bytes == 0 || !sleeper_) {
        return;
    }
    sleeper_(throttle_duration(bytes, bytesPerSecond_));
}

TaskPlan DownloadService::build_plan(const std::string& title, const std::vector<std::string>& steps) const {
    TaskPlan plan;
    plan.id = make_plan_id(title);
    plan.title = title;
    plan.steps.reserve(steps.size());
    for (std::size_t i = 0; i < steps.size(); ++i) {
        plan.steps.push_back(TaskStep{
            .id = "step-" + std::to_string(i + 1),
            .title = steps[i],
            .status = TaskStatus::Pending,
            .progress = 0,
            .detail = {},
        });
    }
    return plan;
}

TaskPlan DownloadService::build_download_plan(const DownloadRequest& request) const {
    auto plan = make_download_plan(request);
    if (!request.expectedHash.empty()) {
        plan.steps[2].detail = "checksum expected";
    }
    return plan;
}

DownloadResult DownloadService::execute(const DownloadRequest& request, TaskQueue* queue) const {
    return execute_single(request, queue);
}

DownloadBatchResult DownloadService::execute_many(const std::vector<DownloadRequest>& requests, TaskQueue* queue) const {
    DownloadBatchResult batch;
    batch.results.resize(requests.size());
    if (requests.empty()) {
        return batch;
    }

    const auto workerCount = static_cast<std::size_t>(std::max(1, std::min(maxConcurrency_, static_cast<int>(requests.size()))));
    std::atomic_size_t nextIndex{0};
    std::vector<std::future<void>> workers;
    workers.reserve(workerCount);

    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        workers.push_back(std::async(std::launch::async, [this, &requests, &batch, &nextIndex, queue]() {
            for (;;) {
                const auto index = nextIndex.fetch_add(1);
                if (index >= requests.size()) {
                    break;
                }
                batch.results[index] = execute_single(requests[index], queue);
            }
        }));
    }

    for (auto& worker : workers) {
        worker.get();
    }

    return batch;
}

DownloadResult DownloadService::execute_single(const DownloadRequest& request, TaskQueue* queue) const {
    DownloadResult result;
    result.plan = build_download_plan(request);
    result.artifact.requestId = result.plan.id;
    result.artifact.title = request.title.empty() ? result.plan.title : request.title;
    const auto urls = candidate_urls(request);
    result.artifact.destination = request.destination;

    if (queue) {
        queue->enqueue(result.plan);
        queue->start(result.plan.id);
    }

    TaskPipeline pipeline(result.plan);
    pipeline.start();
    result.state = DownloadState::Running;
    result.logs.push_back("started download plan: " + result.plan.id);

    if (urls.empty()) {
        result.error = "download url is required";
        pipeline.advance_step("prepare", TaskStatus::Failed, result.error);
        if (queue) {
            queue->complete_step(result.plan.id, "prepare", TaskStatus::Failed, result.error);
        }
        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish(result.error);
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.state = DownloadState::Failed;
        result.success = false;
        result.artifact.attempts = 0;
        return result;
    }

    if (request.destination.empty()) {
        result.error = "download destination is required";
        pipeline.advance_step("prepare", TaskStatus::Failed, result.error);
        if (queue) {
            queue->complete_step(result.plan.id, "prepare", TaskStatus::Failed, result.error);
        }
        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish(result.error);
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.state = DownloadState::Failed;
        result.success = false;
        return result;
    }

    std::string prepareError;
    if (!dawn::infra::fs::ensure_parent_directory(request.destination, &prepareError)) {
        result.error = prepareError;
        pipeline.advance_step("prepare", TaskStatus::Failed, result.error);
        if (queue) {
            queue->complete_step(result.plan.id, "prepare", TaskStatus::Failed, result.error);
        }
        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish(result.error);
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.state = DownloadState::Failed;
        result.success = false;
        return result;
    }

    pipeline.advance_step("prepare", TaskStatus::Succeeded, "destination ready");
    if (queue) {
        queue->complete_step(result.plan.id, "prepare", TaskStatus::Succeeded, "destination ready");
    }
    result.logs.push_back("prepare: destination ready");

    const bool resumeRequested = !request.overwriteExisting && std::filesystem::exists(request.destination);
    std::uintmax_t existingBytes = 0;
    if (resumeRequested) {
        std::string sizeError;
        existingBytes = dawn::infra::fs::file_size(request.destination, &sizeError);
        if (!sizeError.empty()) {
            result.error = sizeError;
            pipeline.advance_step("prepare", TaskStatus::Failed, result.error);
            if (queue) {
                queue->complete_step(result.plan.id, "prepare", TaskStatus::Failed, result.error);
            }
            result.plan = pipeline.plan();
            result.taskResult = pipeline.finish(result.error);
            result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
            result.state = DownloadState::Failed;
            result.success = false;
            return result;
        }
        result.logs.push_back("resume from existing file size: " + std::to_string(existingBytes));
    }

    const bool chunkedDownload = request.chunkSizeBytes > 0 && !resumeRequested;
    const int retryCount = std::max(0, request.retryCount);
    const int maxAttempts = retryCount + 1;
    std::string lastError;
    bool downloadSucceeded = false;
    int attempt = 0;

    auto perform_standard_download = [&](const std::string& url, int retry) -> bool {
        auto downloadRequest = make_get_request(url);
        if (resumeRequested) {
            downloadRequest.headers.emplace("Range", "bytes=" + std::to_string(existingBytes) + "-");
        }

        dawn::infra::net::HttpResponse response;
        {
            std::lock_guard<std::mutex> lock(clientMutex_);
            response = client_->send(downloadRequest);
        }

        if (!response.success()) {
            lastError = "http status " + std::to_string(response.statusCode);
            result.logs.push_back(make_download_error("download failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        result.artifact.sourceUrl = url;
        const bool appendResponse = resumeRequested && response.statusCode == 206;
        std::string writeError;
        if (!write_download_segment(request.destination, response.body, appendResponse, &writeError)) {
            lastError = writeError;
            result.logs.push_back(make_download_error(appendResponse ? "append failed from " + url : "write failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        throttle(response.body.size());
        result.artifact.bytesWritten = appendResponse
            ? static_cast<std::size_t>(existingBytes + response.body.size())
            : response.body.size();
        result.chunks.push_back(DownloadChunkResult{
            DownloadChunk{
                0,
                appendResponse ? existingBytes : 0,
                appendResponse
                    ? (response.body.empty() ? existingBytes : existingBytes + response.body.size() - 1)
                    : (response.body.empty() ? 0 : response.body.size() - 1),
            },
            url,
            response.body.size(),
            dawn::infra::hash::sha256_hex(response.body),
            true,
            {},
        });
        result.logs.push_back("download wrote " + std::to_string(result.artifact.bytesWritten) + " bytes from " + url);
        return true;
    };

    auto perform_chunked_download = [&](const std::string& url, int retry) -> bool {
        std::uintmax_t currentOffset = 0;
        std::optional<std::uintmax_t> totalSize;
        std::size_t chunkIndex = 0;

        while (true) {
            const auto chunkSize = std::max<std::size_t>(1, request.chunkSizeBytes);
            const std::uintmax_t chunkStart = currentOffset;
            const std::uintmax_t chunkEnd = totalSize
                ? std::min<std::uintmax_t>(*totalSize - 1, chunkStart + static_cast<std::uintmax_t>(chunkSize) - 1)
                : chunkStart + static_cast<std::uintmax_t>(chunkSize) - 1;

            auto downloadRequest = make_get_request(url);
            downloadRequest.headers.emplace("Range", "bytes=" + std::to_string(chunkStart) + "-" + std::to_string(chunkEnd));

            dawn::infra::net::HttpResponse response;
            {
                std::lock_guard<std::mutex> lock(clientMutex_);
                response = client_->send(downloadRequest);
            }

            if (!response.success()) {
                lastError = "http status " + std::to_string(response.statusCode);
                result.logs.push_back(make_download_error("chunk download failed from " + url, retry, maxAttempts, lastError));
                return false;
            }

            if (response.statusCode != 200 && response.statusCode != 206) {
                lastError = "unexpected http status " + std::to_string(response.statusCode);
                result.logs.push_back(make_download_error("chunk download failed from " + url, retry, maxAttempts, lastError));
                return false;
            }
            if (response.statusCode == 200 && chunkIndex > 0) {
                lastError = "server ignored range request for chunk " + std::to_string(chunkIndex);
                result.logs.push_back(make_download_error("chunk download failed from " + url, retry, maxAttempts, lastError));
                return false;
            }

            std::string writeError;
            const bool appendChunk = chunkIndex > 0;
            if (!write_download_segment(request.destination, response.body, appendChunk, &writeError)) {
                lastError = writeError;
                result.logs.push_back(make_download_error(appendChunk ? "append failed from " + url : "write failed from " + url, retry, maxAttempts, lastError));
                return false;
            }

            throttle(response.body.size());

            DownloadChunkResult chunkResult;
            chunkResult.chunk = DownloadChunk{
                chunkIndex,
                chunkStart,
                chunkStart + (response.body.empty() ? 0 : static_cast<std::uintmax_t>(response.body.size()) - 1),
            };
            chunkResult.sourceUrl = url;
            chunkResult.bytesWritten = response.body.size();
            chunkResult.checksum = dawn::infra::hash::sha256_hex(response.body);
            chunkResult.success = true;
            result.chunks.push_back(std::move(chunkResult));
            result.artifact.sourceUrl = url;
            result.artifact.bytesWritten += response.body.size();
            result.logs.push_back("chunk " + std::to_string(chunkIndex) + " from " + url + " wrote " + std::to_string(response.body.size()) + " bytes");

            if (response.statusCode == 206) {
                if (const auto contentRange = find_header_value(response, "Content-Range"); contentRange.has_value()) {
                    totalSize = parse_total_size_from_content_range(*contentRange);
                }
            } else {
                totalSize = response.body.size();
            }

            if (response.body.empty()) {
                break;
            }
            currentOffset += response.body.size();
            ++chunkIndex;

            if (totalSize.has_value() && currentOffset >= *totalSize) {
                break;
            }
            if (!totalSize.has_value() && response.body.size() < chunkSize) {
                break;
            }
        }

        return !result.chunks.empty();
    };

    for (const auto& url : urls) {
        result.logs.push_back("download source: " + url);
        for (int retry = 1; retry <= maxAttempts; ++retry) {
            ++attempt;
            result.artifact.attempts = attempt;
            result.logs.push_back(make_step_detail("download attempt from " + url, retry, maxAttempts));
            result.chunks.clear();
            result.artifact.bytesWritten = 0;
            result.artifact.checksum.clear();
            result.artifact.verified = false;
            result.artifact.sourceUrl = url;

            const bool sourceSucceeded = chunkedDownload
                ? perform_chunked_download(url, retry)
                : perform_standard_download(url, retry);

            if (sourceSucceeded) {
                pipeline.advance_step("download", TaskStatus::Succeeded, chunkedDownload ? "chunked download completed" : "download completed");
                if (queue) {
                    queue->complete_step(result.plan.id, "download", TaskStatus::Succeeded, chunkedDownload ? "chunked download completed" : "download completed");
                }
                downloadSucceeded = true;
                break;
            }
            if (retry < maxAttempts) {
                continue;
            }
            lastError = make_download_error("source exhausted", retry, maxAttempts, lastError);
        }
        if (!downloadSucceeded && url != urls.back()) {
            result.logs.push_back("falling back to next mirror");
        }
        if (downloadSucceeded) {
            break;
        }
    }

    if (!downloadSucceeded) {
        result.error = lastError.empty() ? "download failed" : lastError;
        pipeline.advance_step("download", TaskStatus::Failed, result.error);
        if (queue) {
            queue->complete_step(result.plan.id, "download", TaskStatus::Failed, result.error);
        }
        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish(result.error);
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.state = DownloadState::Failed;
        result.success = false;
        return result;
    }

    result.artifact.checksum = dawn::infra::hash::sha256_file_hex(request.destination, &lastError);
    if (result.artifact.checksum.empty()) {
        result.error = lastError.empty() ? "failed to compute checksum" : lastError;
        pipeline.advance_step("verify", TaskStatus::Failed, result.error);
        if (queue) {
            queue->complete_step(result.plan.id, "verify", TaskStatus::Failed, result.error);
        }
        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish(result.error);
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.state = DownloadState::Failed;
        result.success = false;
        return result;
    }

    if (!request.expectedHash.empty()) {
        if (!dawn::infra::hash::compare_hash(request.expectedHash, result.artifact.checksum)) {
            result.artifact.verified = false;
            result.error = "hash mismatch";
            result.logs.push_back("expected hash: " + request.expectedHash);
            result.logs.push_back("actual hash:   " + result.artifact.checksum);
            pipeline.advance_step("verify", TaskStatus::Failed, result.error);
            if (queue) {
                queue->complete_step(result.plan.id, "verify", TaskStatus::Failed, result.error);
            }
            result.plan = pipeline.plan();
            result.taskResult = pipeline.finish(result.error);
            result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
            result.state = DownloadState::Failed;
            result.success = false;
            return result;
        }
        result.artifact.verified = true;
        pipeline.advance_step("verify", TaskStatus::Succeeded, "hash verified");
        if (queue) {
            queue->complete_step(result.plan.id, "verify", TaskStatus::Succeeded, "hash verified");
        }
        result.logs.push_back("hash verified");
    } else {
        result.artifact.verified = true;
        pipeline.advance_step("verify", TaskStatus::Succeeded, "hash skipped");
        if (queue) {
            queue->complete_step(result.plan.id, "verify", TaskStatus::Succeeded, "hash skipped");
        }
        result.logs.push_back("hash skipped");
    }

    pipeline.advance_step("finalize", TaskStatus::Succeeded, "download ready");
    if (queue) {
        queue->complete_step(result.plan.id, "finalize", TaskStatus::Succeeded, "download ready");
    }
    result.logs.push_back("finalize: download ready");

    result.plan = pipeline.plan();
    result.taskResult = pipeline.finish("download completed");
    result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
    result.state = DownloadState::Completed;
    result.success = true;
    return result;
}

UpdateSimulationResult DownloadService::simulate_update(const UpdateSimulationRequest& request) const {
    UpdateSimulationResult result;
    result.reversible = true;
    result.summary = "Update simulation for instance " + request.instanceId + ", project " + request.projectId + " from " + request.currentVersionId + " to " + request.targetVersionId;
    result.upgradeItems.push_back(request.projectId + "@" + request.targetVersionId);

    for (const auto& dependency : request.targetDependencies) {
        if (std::find(request.currentDependencies.begin(), request.currentDependencies.end(), dependency) == request.currentDependencies.end()) {
            result.dependencyChanges.push_back("add dependency: " + dependency);
        }
    }
    for (const auto& dependency : request.currentDependencies) {
        if (std::find(request.targetDependencies.begin(), request.targetDependencies.end(), dependency) == request.targetDependencies.end()) {
            result.dependencyChanges.push_back("remove dependency: " + dependency);
        }
    }
    if (request.currentVersionId != request.targetVersionId) {
        result.incompatibleItems.push_back("version delta may require a restart");
    }
    if (request.targetDependencies.size() > request.currentDependencies.size()) {
        result.incompatibleItems.push_back("dependency graph expanded");
    }
    return result;
}

} // namespace dawn::core
