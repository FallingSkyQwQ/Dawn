#pragma once

#include "dawn/core/model/enums.h"

#include <string>
#include <vector>

namespace dawn::core {

struct TaskStep {
    std::string id;
    std::string title;
    TaskStatus status = TaskStatus::Pending;
    int progress = 0;
    std::string detail;
};

struct TaskPlan {
    std::string id;
    std::string title;
    TaskStatus status = TaskStatus::Pending;
    std::vector<TaskStep> steps;
    std::string createdAt;
    std::string updatedAt;
};

struct TaskResult {
    std::string planId;
    TaskStatus status = TaskStatus::Pending;
    std::string summary;
    std::vector<std::string> logs;
};

} // namespace dawn::core
