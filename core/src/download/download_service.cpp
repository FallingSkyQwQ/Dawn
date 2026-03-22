#include "dawn/core/download/download_service.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <utility>

namespace dawn::core {

namespace {

std::string make_download_id() {
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "download-" + std::to_string(millis);
}

std::string make_plan_id(const std::string& title) {
    std::string id = "plan-";
    for (char ch : title) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            id.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!id.empty() && id.back() != '-') {
            id.push_back('-');
        }
    }
    if (id == "plan-") {
        id += "download";
    }
    id.push_back('-');
    id += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return id;
}

} // namespace

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
