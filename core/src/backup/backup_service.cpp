#include "dawn/core/backup/backup_service.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;

Value snapshot_to_json(const SnapshotMetadata& snapshot) {
    Value::Object object;
    object.emplace("snapshotId", snapshot.snapshotId);
    object.emplace("instanceId", snapshot.instanceId);
    object.emplace("label", snapshot.label);
    object.emplace("createdAt", snapshot.createdAt);
    object.emplace("archivePath", snapshot.archivePath.generic_string());
    object.emplace("reversible", snapshot.reversible);
    object.emplace("note", snapshot.note);
    return Value(std::move(object));
}

SnapshotMetadata snapshot_from_json(const Value& value) {
    SnapshotMetadata snapshot;
    if (!value.is_object()) {
        return snapshot;
    }
    const auto& object = value.as_object();
    if (const auto* entry = dawn::infra::json::find(object, "snapshotId"); entry && entry->is_string()) snapshot.snapshotId = entry->as_string();
    if (const auto* entry = dawn::infra::json::find(object, "instanceId"); entry && entry->is_string()) snapshot.instanceId = entry->as_string();
    if (const auto* entry = dawn::infra::json::find(object, "label"); entry && entry->is_string()) snapshot.label = entry->as_string();
    if (const auto* entry = dawn::infra::json::find(object, "createdAt"); entry && entry->is_string()) snapshot.createdAt = entry->as_string();
    if (const auto* entry = dawn::infra::json::find(object, "archivePath"); entry && entry->is_string()) snapshot.archivePath = std::filesystem::path(entry->as_string());
    if (const auto* entry = dawn::infra::json::find(object, "reversible"); entry && entry->is_bool()) snapshot.reversible = entry->as_bool();
    if (const auto* entry = dawn::infra::json::find(object, "note"); entry && entry->is_string()) snapshot.note = entry->as_string();
    return snapshot;
}

} // namespace

BackupService::BackupService(std::filesystem::path root) : root_(std::move(root)) {}

SnapshotMetadata BackupService::create_snapshot_manifest(const InstanceManifest& manifest, const std::string& label, std::string* error) {
    SnapshotMetadata snapshot;
    snapshot.snapshotId = make_snapshot_id(manifest.id);
    snapshot.instanceId = manifest.id;
    snapshot.label = label.empty() ? "manual snapshot" : label;
    snapshot.createdAt = timestamp_now();
    snapshot.archivePath = snapshot_directory(manifest.id) / snapshot.snapshotId;
    snapshot.reversible = true;

    const auto metadataPath = snapshot_directory(manifest.id) / (snapshot.snapshotId + ".json");
    std::error_code ec;
    std::filesystem::create_directories(snapshot.archivePath, ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return SnapshotMetadata{};
    }

    std::size_t copiedEntries = 0;
    const auto sourceGameDir = std::filesystem::path(manifest.gameDir);
    if (!manifest.gameDir.empty() && std::filesystem::exists(sourceGameDir, ec) && !ec) {
        const auto destination = snapshot.archivePath / "game";
        std::filesystem::create_directories(destination, ec);
        if (ec) {
            if (error) {
                *error = ec.message();
            }
            return SnapshotMetadata{};
        }
        for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceGameDir, ec)) {
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return SnapshotMetadata{};
            }
            const auto relative = std::filesystem::relative(entry.path(), sourceGameDir, ec);
            if (ec) {
                continue;
            }
            const auto target = destination / relative;
            if (entry.is_directory()) {
                std::filesystem::create_directories(target, ec);
                if (ec) {
                    if (error) {
                        *error = ec.message();
                    }
                    return SnapshotMetadata{};
                }
                continue;
            }
            std::filesystem::create_directories(target.parent_path(), ec);
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return SnapshotMetadata{};
            }
            std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return SnapshotMetadata{};
            }
            ++copiedEntries;
        }
    }
    snapshot.note = "copied " + std::to_string(copiedEntries) + " file(s)";

    if (!dawn::infra::fs::write_text_file(metadataPath, dawn::infra::json::stringify(snapshot_to_json(snapshot), 2), error)) {
        return SnapshotMetadata{};
    }
    return snapshot;
}

bool BackupService::restore_snapshot(const SnapshotMetadata& snapshot, const std::filesystem::path& targetGameDir, std::string* error) const {
    const auto sourceGameDir = snapshot.archivePath / "game";
    std::error_code ec;
    if (!std::filesystem::exists(sourceGameDir, ec) || ec) {
        if (error) {
            *error = "snapshot game payload not found";
        }
        return false;
    }

    std::filesystem::create_directories(targetGameDir, ec);
    if (ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(sourceGameDir, ec)) {
        if (ec) {
            if (error) {
                *error = ec.message();
            }
            return false;
        }
        const auto relative = std::filesystem::relative(entry.path(), sourceGameDir, ec);
        if (ec) {
            continue;
        }
        const auto target = targetGameDir / relative;
        if (entry.is_directory()) {
            std::filesystem::create_directories(target, ec);
            if (ec) {
                if (error) {
                    *error = ec.message();
                }
                return false;
            }
            continue;
        }
        std::filesystem::create_directories(target.parent_path(), ec);
        if (ec) {
            if (error) {
                *error = ec.message();
            }
            return false;
        }
        std::filesystem::copy_file(entry.path(), target, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            if (error) {
                *error = ec.message();
            }
            return false;
        }
    }
    return true;
}

std::vector<SnapshotMetadata> BackupService::list_snapshots(const std::string& instanceId, std::string* error) const {
    std::vector<SnapshotMetadata> result;
    const auto directory = snapshot_directory(instanceId);
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
        result.push_back(snapshot_from_json(parsed.value));
    }
    return result;
}

RestorePlan BackupService::build_restore_plan(const SnapshotMetadata& snapshot) const {
    RestorePlan plan;
    plan.snapshotId = snapshot.snapshotId;
    plan.reversible = snapshot.reversible;
    plan.summary = "Restore snapshot " + snapshot.snapshotId + " for instance " + snapshot.instanceId;
    plan.taskPlan.id = "restore-" + snapshot.snapshotId;
    plan.taskPlan.title = "Restore " + snapshot.label;
    plan.taskPlan.steps = {
        {"validate", "Validate snapshot metadata", TaskStatus::Pending, 0, {}},
        {"restore", "Restore metadata and files", TaskStatus::Pending, 0, {}},
        {"verify", "Verify restored instance state", TaskStatus::Pending, 0, {}},
    };
    return plan;
}

std::filesystem::path BackupService::snapshot_directory(const std::string& instanceId) const {
    return root_ / "snapshots" / instanceId;
}

std::string BackupService::make_snapshot_id(const std::string& instanceId) const {
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return instanceId + "-snapshot-" + std::to_string(millis);
}

std::string BackupService::timestamp_now() const {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

} // namespace dawn::core
