#pragma once

#include "dawn/core/model/task_types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace dawn::core {

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
    std::string enqueue(DownloadJob job);
    std::vector<DownloadJob> jobs() const;
    bool pause(const std::string& id);
    bool resume(const std::string& id);
    bool mark_complete(const std::string& id);
    TaskPlan build_plan(const std::string& title, const std::vector<std::string>& steps) const;
    UpdateSimulationResult simulate_update(const UpdateSimulationRequest& request) const;

private:
    std::vector<DownloadJob> jobs_;
};

} // namespace dawn::core
