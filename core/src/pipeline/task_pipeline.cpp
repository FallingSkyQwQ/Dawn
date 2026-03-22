#include "dawn/core/pipeline/task_pipeline.h"

#include <algorithm>
#include <utility>

namespace dawn::core {

TaskPipeline::TaskPipeline(TaskPlan plan) : plan_(std::move(plan)) {}

const TaskPlan& TaskPipeline::plan() const noexcept {
    return plan_;
}

void TaskPipeline::start() {
    plan_.status = TaskStatus::Running;
}

void TaskPipeline::pause() {
    if (!is_terminal(plan_.status)) {
        plan_.status = TaskStatus::Paused;
    }
}

void TaskPipeline::resume() {
    if (plan_.status == TaskStatus::Paused) {
        plan_.status = TaskStatus::Running;
    }
}

void TaskPipeline::advance_step(const std::string& stepId, TaskStatus status, std::string detail) {
    auto it = std::find_if(plan_.steps.begin(), plan_.steps.end(), [&](const TaskStep& step) {
        return step.id == stepId;
    });
    if (it == plan_.steps.end()) {
        return;
    }

    it->status = status;
    it->detail = std::move(detail);

    const bool all_succeeded = std::all_of(plan_.steps.begin(), plan_.steps.end(), [](const TaskStep& step) {
        return step.status == TaskStatus::Succeeded;
    });
    const bool any_failed = std::any_of(plan_.steps.begin(), plan_.steps.end(), [](const TaskStep& step) {
        return step.status == TaskStatus::Failed || step.status == TaskStatus::Cancelled;
    });

    if (any_failed) {
        plan_.status = TaskStatus::Failed;
    } else if (all_succeeded) {
        plan_.status = TaskStatus::Succeeded;
    } else if (plan_.status != TaskStatus::Paused) {
        plan_.status = TaskStatus::Running;
    }
}

void TaskPipeline::mark_failed(std::string detail) {
    plan_.status = TaskStatus::Failed;
    if (!plan_.steps.empty()) {
        plan_.steps.back().status = TaskStatus::Failed;
        plan_.steps.back().detail = std::move(detail);
    }
}

TaskResult TaskPipeline::finish(std::string summary) const {
    TaskResult result;
    result.planId = plan_.id;
    result.status = plan_.status;
    result.summary = std::move(summary);
    for (const auto& step : plan_.steps) {
        result.logs.push_back(step.id + ":" + std::string(to_string(step.status)));
    }
    return result;
}

} // namespace dawn::core
