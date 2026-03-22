#pragma once

#include "dawn/core/model/task_types.h"
#include "dawn/infra/net/http_client.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace dawn::core {

class TaskQueue;

enum class DownloadState {
    Queued,
    Running,
    Paused,
    Completed,
    Failed,
};

struct DownloadJob {
    std::string id;
    std::string title;
    std::string url;
    std::filesystem::path destination;
    std::string checksum;
    DownloadState state = DownloadState::Queued;
    std::vector<std::string> notes;
};

struct DownloadRequest {
    std::string id;
    std::string title;
    std::string url;
    std::vector<std::string> mirrors;
    std::filesystem::path destination;
    std::string expectedHash;
    int retryCount = 0;
    bool overwriteExisting = true;
};

struct DownloadArtifact {
    std::string requestId;
    std::string title;
    std::string sourceUrl;
    std::filesystem::path destination;
    std::string checksum;
    std::size_t bytesWritten = 0;
    int attempts = 0;
    bool verified = false;
};

struct DownloadResult {
    bool success = false;
    DownloadState state = DownloadState::Queued;
    DownloadArtifact artifact;
    TaskPlan plan;
    TaskResult taskResult;
    std::vector<std::string> logs;
    std::string error;
};

struct DownloadBatchResult {
    std::vector<DownloadResult> results;
};

struct UpdateSimulationRequest {
    std::string instanceId;
    std::string projectId;
    std::string currentVersionId;
    std::string targetVersionId;
    std::vector<std::string> currentDependencies;
    std::vector<std::string> targetDependencies;
};

struct UpdateSimulationResult {
    std::vector<std::string> upgradeItems;
    std::vector<std::string> dependencyChanges;
    std::vector<std::string> incompatibleItems;
    bool reversible = true;
    std::string summary;
};

class DownloadService {
public:
    DownloadService();
    explicit DownloadService(std::shared_ptr<dawn::infra::net::HttpClient> client);
    explicit DownloadService(int maxConcurrency);
    DownloadService(std::shared_ptr<dawn::infra::net::HttpClient> client, int maxConcurrency);

    std::string enqueue(DownloadJob job);
    std::vector<DownloadJob> jobs() const;
    bool pause(const std::string& id);
    bool resume(const std::string& id);
    bool mark_complete(const std::string& id);
    TaskPlan build_plan(const std::string& title, const std::vector<std::string>& steps) const;
    TaskPlan build_download_plan(const DownloadRequest& request) const;
    DownloadResult execute(const DownloadRequest& request, TaskQueue* queue = nullptr) const;
    DownloadBatchResult execute_many(const std::vector<DownloadRequest>& requests, TaskQueue* queue = nullptr) const;
    void set_max_concurrency(int maxConcurrency);
    [[nodiscard]] int max_concurrency() const noexcept;
    UpdateSimulationResult simulate_update(const UpdateSimulationRequest& request) const;

private:
    [[nodiscard]] static std::string make_download_id();
    [[nodiscard]] static std::string make_plan_id(const std::string& title);
    [[nodiscard]] static std::string make_step_detail(const std::string& message, int attempt, int maxAttempts);
    [[nodiscard]] std::vector<std::string> candidate_urls(const DownloadRequest& request) const;
    [[nodiscard]] DownloadResult execute_single(const DownloadRequest& request, TaskQueue* queue) const;

    std::shared_ptr<dawn::infra::net::HttpClient> client_;
    int maxConcurrency_ = 4;
    mutable std::mutex clientMutex_;
    std::vector<DownloadJob> jobs_;
};

} // namespace dawn::core
