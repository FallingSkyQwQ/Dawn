#include "dawn/core/download/download_service.h"

#include "dawn/core/pipeline/task_pipeline.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"
#include "dawn/infra/net/http_client_factory.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <filesystem>
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

} // namespace

DownloadService::DownloadService()
    : client_(dawn::infra::net::HttpClientFactory::create_default_http_client()) {
}

DownloadService::DownloadService(std::shared_ptr<dawn::infra::net::HttpClient> client)
    : client_(client ? std::move(client) : dawn::infra::net::HttpClientFactory::create_default_http_client()) {
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
    DownloadResult result;
    result.plan = build_download_plan(request);
    result.artifact.requestId = result.plan.id;
    result.artifact.title = request.title.empty() ? result.plan.title : request.title;
    result.artifact.sourceUrl = request.url;
    result.artifact.destination = request.destination;

    if (queue) {
        queue->enqueue(result.plan);
        queue->start(result.plan.id);
    }

    TaskPipeline pipeline(result.plan);
    pipeline.start();
    result.state = DownloadState::Running;
    result.logs.push_back("started download plan: " + result.plan.id);

    if (request.url.empty()) {
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

    const int retryCount = std::max(0, request.retryCount);
    const int maxAttempts = retryCount + 1;
    std::string lastError;
    bool downloadSucceeded = false;

    for (int attempt = 1; attempt <= maxAttempts; ++attempt) {
        result.artifact.attempts = attempt;
        result.logs.push_back(make_step_detail("download attempt", attempt, maxAttempts));

        auto downloadRequest = make_get_request(request.url);
        if (resumeRequested) {
            downloadRequest.headers.emplace("Range", "bytes=" + std::to_string(existingBytes) + "-");
        }

        const auto response = client_->send(downloadRequest);
        if (!response.success()) {
            lastError = "http status " + std::to_string(response.statusCode);
            result.logs.push_back(make_download_error("download failed", attempt, maxAttempts, lastError));
            if (attempt < maxAttempts) {
                continue;
            }
            break;
        }

        std::string writeError;
        const bool appendResponse = resumeRequested && response.statusCode == 206;
        if (appendResponse) {
            if (!dawn::infra::fs::append_binary_file(request.destination, response.body, &writeError)) {
                lastError = writeError;
                result.logs.push_back(make_download_error("append failed", attempt, maxAttempts, lastError));
                if (attempt < maxAttempts) {
                    continue;
                }
                break;
            }
        } else if (!dawn::infra::fs::write_binary_file(request.destination, response.body, &writeError)) {
            lastError = writeError;
            result.logs.push_back(make_download_error("write failed", attempt, maxAttempts, lastError));
            if (attempt < maxAttempts) {
                continue;
            }
            break;
        }

        result.artifact.bytesWritten = appendResponse
            ? static_cast<std::size_t>(existingBytes + response.body.size())
            : response.body.size();
        result.logs.push_back("download wrote " + std::to_string(result.artifact.bytesWritten) + " bytes");
        pipeline.advance_step("download", TaskStatus::Succeeded, "download completed");
        if (queue) {
            queue->complete_step(result.plan.id, "download", TaskStatus::Succeeded, "download completed");
        }
        downloadSucceeded = true;
        break;
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
