#include "dawn/core/storage/task_history_repository.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <utility>

namespace dawn::core {

namespace {

infra::json::Value task_result_to_json(const TaskResult& result) {
    infra::json::Value::Object object;
    object.emplace("planId", result.planId);
    object.emplace("status", std::string(to_string(result.status)));
    object.emplace("summary", result.summary);
    infra::json::Value::Array logs;
    for (const auto& log : result.logs) {
        logs.emplace_back(log);
    }
    object.emplace("logs", std::move(logs));
    return infra::json::Value(std::move(object));
}

bool task_result_from_json(const infra::json::Value& value, TaskResult* result) {
    if (!value.is_object() || !result) {
        return false;
    }
    const auto& object = value.as_object();
    const auto* plan = infra::json::find(object, "planId");
    const auto* status = infra::json::find(object, "status");
    const auto* summary = infra::json::find(object, "summary");
    const auto* logs = infra::json::find(object, "logs");
    if (!plan || !plan->is_string() || !status || !status->is_string() || !summary || !summary->is_string()) {
        return false;
    }
    result->planId = plan->as_string();
    result->status = task_status_from_string(status->as_string());
    result->summary = summary->as_string();
    result->logs.clear();
    if (logs && logs->is_array()) {
        for (const auto& entry : logs->as_array()) {
            if (entry.is_string()) {
                result->logs.push_back(entry.as_string());
            }
        }
    }
    return true;
}

} // namespace

TaskHistoryRepository::TaskHistoryRepository(std::filesystem::path root) : root_(std::move(root)) {}

bool TaskHistoryRepository::save(const TaskResult& result, std::string* error) const {
    const auto directory = root_ / "task_history";
    if (!dawn::infra::fs::ensure_directory(directory, error)) {
        return false;
    }
    const auto path = directory / (result.planId + ".json");
    return dawn::infra::fs::write_text_file(path, dawn::infra::json::stringify(task_result_to_json(result), 2), error);
}

std::vector<TaskResult> TaskHistoryRepository::list(std::string* error) const {
    std::vector<TaskResult> result;
    const auto directory = root_ / "task_history";
    for (const auto& path : dawn::infra::fs::list_files(directory, ".json")) {
        std::string text;
        if (!dawn::infra::fs::read_text_file(path, &text, error)) {
            continue;
        }
        const auto parsed = dawn::infra::json::parse(text);
        if (!parsed.ok) {
            if (error) {
                *error = parsed.error.message;
            }
            continue;
        }
        TaskResult task;
        if (task_result_from_json(parsed.value, &task)) {
            result.push_back(std::move(task));
        }
    }
    return result;
}

} // namespace dawn::core
