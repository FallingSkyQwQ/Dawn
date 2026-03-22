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
    std::mutex logMutex;
    auto append_log = [&](std::string message) {
        std::lock_guard<std::mutex> lock(logMutex);
        result.logs.push_back(std::move(message));
    };
    append_log("started download plan: " + result.plan.id);

    if (urls.empty()) {
        result.error = "download url is required";
        pipeline.advance_step("prepare", TaskStatus::Failed, result.error);
        if (queue) {
            queue->complete_step(result.plan.id, "prepare", TaskStatus::Failed, result.error);
        }
        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish(result.error);
        for (const auto& log : result.taskResult.logs) {
            append_log(log);
        }
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
        for (const auto& log : result.taskResult.logs) {
            append_log(log);
        }
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
        for (const auto& log : result.taskResult.logs) {
            append_log(log);
        }
        result.state = DownloadState::Failed;
        result.success = false;
        return result;
    }

    pipeline.advance_step("prepare", TaskStatus::Succeeded, "destination ready");
    if (queue) {
        queue->complete_step(result.plan.id, "prepare", TaskStatus::Succeeded, "destination ready");
    }
    append_log("prepare: destination ready");

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
            for (const auto& log : result.taskResult.logs) {
                append_log(log);
            }
            result.state = DownloadState::Failed;
            result.success = false;
            return result;
        }
        append_log("resume from existing file size: " + std::to_string(existingBytes));
    }

    const bool chunkedDownload = request.chunkSizeBytes > 0 && !resumeRequested;
    const int retryCount = std::max(0, request.retryCount);
    const int maxAttempts = retryCount + 1;
    std::string lastError;
    bool downloadSucceeded = false;
    int attempt = 0;
    struct SharedRateLimiter {
        std::size_t bytesPerSecond = 0;
        DownloadService::Sleeper sleeper;
        mutable std::mutex mutex;
        std::chrono::steady_clock::time_point nextAvailable = std::chrono::steady_clock::now();

        void consume(std::size_t bytes) {
            if (bytesPerSecond == 0 || bytes == 0 || !sleeper) {
                return;
            }
            const auto delay = DownloadService::throttle_duration(bytes, bytesPerSecond);
            if (delay.count() <= 0) {
                return;
            }
            std::chrono::steady_clock::time_point wakeTime;
            {
                std::lock_guard<std::mutex> lock(mutex);
                const auto now = std::chrono::steady_clock::now();
                if (nextAvailable < now) {
                    nextAvailable = now;
                }
                nextAvailable += delay;
                wakeTime = nextAvailable;
            }
            const auto now = std::chrono::steady_clock::now();
            if (wakeTime > now) {
                sleeper(std::chrono::duration_cast<std::chrono::milliseconds>(wakeTime - now));
            }
        }
    };
    SharedRateLimiter rateLimiter;
    rateLimiter.bytesPerSecond = bytesPerSecond_;
    rateLimiter.sleeper = sleeper_;
    auto throttle_shared = [&](std::size_t bytes) {
        rateLimiter.consume(bytes);
    };

    auto perform_standard_download = [&](const std::string& url, int retry) -> bool {
        auto downloadRequest = make_get_request(url);
        if (resumeRequested) {
            downloadRequest.headers.emplace("Range", "bytes=" + std::to_string(existingBytes) + "-");
        }

        const auto response = client_->send(downloadRequest);

        if (!response.success()) {
            lastError = "http status " + std::to_string(response.statusCode);
            append_log(make_download_error("download failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        result.artifact.sourceUrl = url;
        const bool appendResponse = resumeRequested && response.statusCode == 206;
        std::string writeError;
        if (!write_download_segment(request.destination, response.body, appendResponse, &writeError)) {
            lastError = writeError;
            append_log(make_download_error(appendResponse ? "append failed from " + url : "write failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        throttle_shared(response.body.size());
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
            response.body,
            dawn::infra::hash::sha256_hex(response.body),
            true,
            {},
        });
        append_log("download wrote " + std::to_string(result.artifact.bytesWritten) + " bytes from " + url);
        return true;
    };

    auto perform_chunked_download = [&](const std::string& url, int retry) -> bool {
        const auto chunkSize = std::max<std::size_t>(1, request.chunkSizeBytes);
        auto probeRequest = make_get_request(url);
        probeRequest.headers.emplace("Range", "bytes=0-" + std::to_string(chunkSize - 1));
        const auto probeResponse = client_->send(probeRequest);
        if (!probeResponse.success()) {
            lastError = "http status " + std::to_string(probeResponse.statusCode);
            append_log(make_download_error("chunk probe failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        std::vector<DownloadChunkResult> chunkResults;
        std::vector<std::string> chunkBodies;

        auto finalize_chunk_download = [&](const std::vector<DownloadChunkResult>& orderedChunks, const std::vector<std::string>& bodies) -> bool {
            const std::filesystem::path tempPath = request.destination.string() + ".partial";
            std::string writeError;
            if (!bodies.empty()) {
                if (!write_download_segment(tempPath, bodies.front(), false, &writeError)) {
                    lastError = writeError;
                    append_log(make_download_error("chunk merge failed from " + url, retry, maxAttempts, lastError));
                    std::error_code ec;
                    std::filesystem::remove(tempPath, ec);
                    return false;
                }
                for (std::size_t index = 1; index < bodies.size(); ++index) {
                    if (!write_download_segment(tempPath, bodies[index], true, &writeError)) {
                        lastError = writeError;
                        append_log(make_download_error("chunk merge failed from " + url, retry, maxAttempts, lastError));
                        std::error_code ec;
                        std::filesystem::remove(tempPath, ec);
                        return false;
                    }
                }
            }

            std::error_code ec;
            std::filesystem::copy_file(tempPath, request.destination, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                lastError = ec.message();
                append_log(make_download_error("final copy failed from " + url, retry, maxAttempts, lastError));
                std::error_code removeError;
                std::filesystem::remove(tempPath, removeError);
                return false;
            }
            std::error_code removeError;
            std::filesystem::remove(tempPath, removeError);

            result.chunks = orderedChunks;
            result.artifact.sourceUrl = url;
            result.artifact.bytesWritten = 0;
            for (const auto& chunk : orderedChunks) {
                result.artifact.bytesWritten += chunk.bytesWritten;
            }
            append_log("chunked download merged " + std::to_string(result.artifact.bytesWritten) + " bytes from " + url);
            return true;
        };

        if (probeResponse.statusCode == 200) {
            chunkResults.push_back(DownloadChunkResult{
                DownloadChunk{0, 0, probeResponse.body.empty() ? 0 : static_cast<std::uintmax_t>(probeResponse.body.size() - 1)},
                url,
                probeResponse.body.size(),
                probeResponse.body,
                dawn::infra::hash::sha256_hex(probeResponse.body),
                true,
                {},
            });
            chunkBodies.push_back(probeResponse.body);
            throttle_shared(probeResponse.body.size());
            append_log("chunk 0 from " + url + " wrote " + std::to_string(probeResponse.body.size()) + " bytes");
            if (!finalize_chunk_download(chunkResults, chunkBodies)) {
                return false;
            }
            return true;
        }

        if (probeResponse.statusCode != 206) {
            lastError = "unexpected http status " + std::to_string(probeResponse.statusCode);
            append_log(make_download_error("chunk probe failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        const auto contentRange = find_header_value(probeResponse, "Content-Range");
        if (!contentRange.has_value()) {
            lastError = "missing Content-Range header";
            append_log(make_download_error("chunk probe failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        const auto totalSize = parse_total_size_from_content_range(*contentRange);
        if (!totalSize.has_value() || *totalSize == 0) {
            lastError = "invalid total size in Content-Range";
            append_log(make_download_error("chunk probe failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        const auto plannedChunks = build_chunk_plan(*totalSize, chunkSize, 0);
        if (plannedChunks.empty()) {
            lastError = "chunk plan is empty";
            append_log(make_download_error("chunk probe failed from " + url, retry, maxAttempts, lastError));
            return false;
        }

        chunkResults.resize(plannedChunks.size());
        chunkBodies.resize(plannedChunks.size());
        chunkResults[0] = DownloadChunkResult{
            plannedChunks.front(),
            url,
            probeResponse.body.size(),
            probeResponse.body,
            dawn::infra::hash::sha256_hex(probeResponse.body),
            true,
            {},
        };
        chunkBodies[0] = probeResponse.body;
        throttle_shared(probeResponse.body.size());
        append_log("chunk 0 from " + url + " wrote " + std::to_string(probeResponse.body.size()) + " bytes");

        if (plannedChunks.size() > 1) {
            const std::size_t desiredConcurrency = request.chunkConcurrency > 0
                ? request.chunkConcurrency
                : static_cast<std::size_t>(std::max(1, maxConcurrency_));
            const std::size_t workerCount = std::max<std::size_t>(1, std::min(desiredConcurrency, plannedChunks.size() - 1));
            std::atomic_size_t nextIndex{1};
            std::atomic_bool cancelled{false};
            std::mutex failureMutex;
            std::string chunkFailure;

            auto record_failure = [&](std::string message) {
                std::lock_guard<std::mutex> lock(failureMutex);
                if (chunkFailure.empty()) {
                    chunkFailure = std::move(message);
                }
                cancelled.store(true);
            };

            std::vector<std::future<void>> workers;
            workers.reserve(workerCount);
            for (std::size_t worker = 0; worker < workerCount; ++worker) {
                workers.push_back(std::async(std::launch::async, [&, url, retry]() {
                    for (;;) {
                        if (cancelled.load()) {
                            break;
                        }
                        const auto index = nextIndex.fetch_add(1);
                        if (index >= plannedChunks.size()) {
                            break;
                        }
                        const auto& chunk = plannedChunks[index];
                        auto chunkRequest = make_get_request(url);
                        chunkRequest.headers.emplace("Range", "bytes=" + std::to_string(chunk.start) + "-" + std::to_string(chunk.end));
                        const auto response = client_->send(chunkRequest);
                        if (!response.success() || response.statusCode != 206) {
                            record_failure(make_download_error("chunk download failed from " + url, retry, maxAttempts, "http status " + std::to_string(response.statusCode)));
                            break;
                        }
                        DownloadChunkResult chunkResult;
                        chunkResult.chunk = chunk;
                        chunkResult.sourceUrl = url;
                        chunkResult.bytesWritten = response.body.size();
                        chunkResult.body = response.body;
                        chunkResult.checksum = dawn::infra::hash::sha256_hex(response.body);
                        chunkResult.success = true;
                        chunkResults[index] = std::move(chunkResult);
                        chunkBodies[index] = response.body;
                        throttle_shared(response.body.size());
                        append_log("chunk " + std::to_string(index) + " from " + url + " wrote " + std::to_string(response.body.size()) + " bytes");
                    }
                }));
            }

            for (auto& worker : workers) {
                worker.get();
            }

            if (cancelled.load()) {
                lastError = chunkFailure.empty() ? "chunk download failed" : chunkFailure;
                append_log(lastError);
                return false;
            }

            for (std::size_t index = 0; index < chunkResults.size(); ++index) {
                if (!chunkResults[index].success) {
                    lastError = "missing chunk result at index " + std::to_string(index);
                    append_log(lastError);
                    return false;
                }
            }
        }

        if (!finalize_chunk_download(chunkResults, chunkBodies)) {
            return false;
        }
        return true;
    };

    for (const auto& url : urls) {
        append_log("download source: " + url);
        for (int retry = 1; retry <= maxAttempts; ++retry) {
            ++attempt;
            result.artifact.attempts = attempt;
            append_log(make_step_detail("download attempt from " + url, retry, maxAttempts));
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
            append_log("falling back to next mirror");
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
        for (const auto& log : result.taskResult.logs) {
            append_log(log);
        }
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
        for (const auto& log : result.taskResult.logs) {
            append_log(log);
        }
        result.state = DownloadState::Failed;
        result.success = false;
        return result;
    }

    if (!request.expectedHash.empty()) {
        if (!dawn::infra::hash::compare_hash(request.expectedHash, result.artifact.checksum)) {
            result.artifact.verified = false;
            result.error = "hash mismatch";
            append_log("expected hash: " + request.expectedHash);
            append_log("actual hash:   " + result.artifact.checksum);
            pipeline.advance_step("verify", TaskStatus::Failed, result.error);
            if (queue) {
                queue->complete_step(result.plan.id, "verify", TaskStatus::Failed, result.error);
            }
            result.plan = pipeline.plan();
            result.taskResult = pipeline.finish(result.error);
            for (const auto& log : result.taskResult.logs) {
                append_log(log);
            }
            result.state = DownloadState::Failed;
            result.success = false;
            return result;
        }
        result.artifact.verified = true;
        pipeline.advance_step("verify", TaskStatus::Succeeded, "hash verified");
        if (queue) {
            queue->complete_step(result.plan.id, "verify", TaskStatus::Succeeded, "hash verified");
        }
        append_log("hash verified");
    } else {
        result.artifact.verified = true;
        pipeline.advance_step("verify", TaskStatus::Succeeded, "hash skipped");
        if (queue) {
            queue->complete_step(result.plan.id, "verify", TaskStatus::Succeeded, "hash skipped");
        }
        append_log("hash skipped");
    }

    pipeline.advance_step("finalize", TaskStatus::Succeeded, "download ready");
    if (queue) {
        queue->complete_step(result.plan.id, "finalize", TaskStatus::Succeeded, "download ready");
    }
    append_log("finalize: download ready");

    result.plan = pipeline.plan();
    result.taskResult = pipeline.finish("download completed");
    for (const auto& log : result.taskResult.logs) {
        append_log(log);
    }
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
