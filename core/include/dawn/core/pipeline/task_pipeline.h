#pragma once

#include "dawn/core/model/task_types.h"

#include <string>

namespace dawn::core {

class TaskPipeline {
public:
    explicit TaskPipeline(TaskPlan plan);

    [[nodiscard]] const TaskPlan& plan() const noexcept;

    void start();
    void pause();
    void resume();
    void advance_step(const std::string& stepId, TaskStatus status, std::string detail = {});
    void mark_failed(std::string detail);
    [[nodiscard]] TaskResult finish(std::string summary = {}) const;

private:
    TaskPlan plan_;
};

} // namespace dawn::core
