#pragma once

#include "dawn/core/model/task_types.h"

#include <vector>

namespace dawn::core {

class TaskQueue {
public:
    std::string enqueue(TaskPlan plan);
    [[nodiscard]] const std::vector<TaskPlan>& tasks() const noexcept;
    [[nodiscard]] TaskPlan* find(const std::string& id);
    bool start(const std::string& id);
    bool pause(const std::string& id);
    bool resume(const std::string& id);
    bool complete_step(const std::string& id, const std::string& stepId, TaskStatus status, std::string detail = {});
    bool fail(const std::string& id, std::string detail);
    bool remove(const std::string& id);

private:
    std::vector<TaskPlan> tasks_;
};

} // namespace dawn::core
