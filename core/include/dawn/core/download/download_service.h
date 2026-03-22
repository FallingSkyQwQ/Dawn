#pragma once

#include "dawn/core/model/task_types.h"
#include "dawn/infra/net/http_client.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
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
    std::size_t chunkSizeBytes = 0;
};

struct DownloadChunk {
    std::size_t index = 0;
    std::uintmax_t start = 0;
    std::uintmax_t end = 0;
};

struct DownloadChunkResult {
    DownloadChunk chunk;
    std::string sourceUrl;
    std::size_t bytesWritten = 0;
    std::string checksum;
    bool success = false;
    std::string error;
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
    std::vector<DownloadChunkResult> chunks;
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
    using Sleeper = std::function<void(std::chrono::milliseconds)>;

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
    void set_bytes_per_second(std::size_t bytesPerSecond);
    [[nodiscard]] std::size_t bytes_per_second() const noexcept;
    void set_sleeper(Sleeper sleeper);
    UpdateSimulationResult simulate_update(const UpdateSimulationRequest& request) const;

private:
    [[nodiscard]] static std::string make_download_id();
    [[nodiscard]] static std::string make_plan_id(const std::string& title);
    [[nodiscard]] static std::string make_step_detail(const std::string& message, int attempt, int maxAttempts);
    [[nodiscard]] static std::vector<DownloadChunk> build_chunk_plan(std::uintmax_t totalSize, std::size_t chunkSize, std::uintmax_t startOffset = 0);
    [[nodiscard]] static std::optional<std::uintmax_t> parse_total_size_from_content_range(const std::string& contentRange);
    [[nodiscard]] static std::optional<std::string> find_header_value(const dawn::infra::net::HttpResponse& response, const std::string& headerName);
    [[nodiscard]] static std::chrono::milliseconds throttle_duration(std::size_t bytes, std::size_t bytesPerSecond);
    [[nodiscard]] std::vector<std::string> candidate_urls(const DownloadRequest& request) const;
    [[nodiscard]] DownloadResult execute_single(const DownloadRequest& request, TaskQueue* queue) const;
    [[nodiscard]] bool write_download_segment(const std::filesystem::path& destination, const std::string& data, bool append, std::string* error) const;
    void throttle(std::size_t bytes) const;

    std::shared_ptr<dawn::infra::net::HttpClient> client_;
    int maxConcurrency_ = 4;
    std::size_t bytesPerSecond_ = 0;
    Sleeper sleeper_;
    mutable std::mutex clientMutex_;
    std::vector<DownloadJob> jobs_;
};

} // namespace dawn::core
