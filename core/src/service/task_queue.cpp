#include "dawn/core/service/task_queue.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace dawn::core {

namespace {

std::string make_task_id() {
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return "task-" + std::to_string(millis);
}

} // namespace

std::string TaskQueue::enqueue(TaskPlan plan) {
    if (plan.id.empty()) {
        plan.id = make_task_id();
    }
    if (is_terminal(plan.status)) {
        plan.status = TaskStatus::Pending;
    }
    tasks_.push_back(std::move(plan));
    return tasks_.back().id;
}

const std::vector<TaskPlan>& TaskQueue::tasks() const noexcept {
    return tasks_;
}

TaskPlan* TaskQueue::find(const std::string& id) {
    const auto it = std::find_if(tasks_.begin(), tasks_.end(), [&](const TaskPlan& plan) {
        return plan.id == id;
    });
    return it == tasks_.end() ? nullptr : &*it;
}

bool TaskQueue::start(const std::string& id) {
    auto* task = find(id);
    if (!task) {
        return false;
    }
    task->status = TaskStatus::Running;
    return true;
}

bool TaskQueue::pause(const std::string& id) {
    auto* task = find(id);
    if (!task || is_terminal(task->status)) {
        return false;
    }
    task->status = TaskStatus::Paused;
    return true;
}

bool TaskQueue::resume(const std::string& id) {
    auto* task = find(id);
    if (!task || task->status != TaskStatus::Paused) {
        return false;
    }
    task->status = TaskStatus::Running;
    return true;
}

bool TaskQueue::complete_step(const std::string& id, const std::string& stepId, TaskStatus status, std::string detail) {
    auto* task = find(id);
    if (!task) {
        return false;
    }

    const auto it = std::find_if(task->steps.begin(), task->steps.end(), [&](const TaskStep& step) {
        return step.id == stepId;
    });
    if (it == task->steps.end()) {
        return false;
    }

    it->status = status;
    it->detail = std::move(detail);
    if (status == TaskStatus::Failed || status == TaskStatus::Cancelled) {
        task->status = TaskStatus::Failed;
        return true;
    }

    const bool all_done = std::all_of(task->steps.begin(), task->steps.end(), [](const TaskStep& step) {
        return step.status == TaskStatus::Succeeded;
    });
    task->status = all_done ? TaskStatus::Succeeded : TaskStatus::Running;
    return true;
}

bool TaskQueue::fail(const std::string& id, std::string detail) {
    auto* task = find(id);
    if (!task) {
        return false;
    }
    task->status = TaskStatus::Failed;
    if (!task->steps.empty()) {
        task->steps.back().status = TaskStatus::Failed;
        task->steps.back().detail = std::move(detail);
    }
    return true;
}

bool TaskQueue::remove(const std::string& id) {
    const auto it = std::remove_if(tasks_.begin(), tasks_.end(), [&](const TaskPlan& plan) {
        return plan.id == id;
    });
    if (it == tasks_.end()) {
        return false;
    }
    tasks_.erase(it, tasks_.end());
    return true;
}

} // namespace dawn::core
