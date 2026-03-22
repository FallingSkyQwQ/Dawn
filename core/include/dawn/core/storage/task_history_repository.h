#pragma once

#include "dawn/core/model/task_types.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dawn::core {

class TaskHistoryRepository {
public:
    explicit TaskHistoryRepository(std::filesystem::path root);

    bool save(const TaskResult& result, std::string* error = nullptr) const;
    std::vector<TaskResult> list(std::string* error = nullptr) const;

private:
    std::filesystem::path root_;
};

} // namespace dawn::core
