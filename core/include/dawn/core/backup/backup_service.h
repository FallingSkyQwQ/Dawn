#pragma once

#include "dawn/core/model/instance_manifest.h"
#include "dawn/core/model/task_types.h"

#include <filesystem>
#include <string>
#include <vector>

namespace dawn::core {

struct SnapshotMetadata {
    std::string snapshotId;
    std::string instanceId;
    std::string label;
    std::string createdAt;
    std::filesystem::path archivePath;
    bool reversible = true;
    std::string note;
};

struct RestorePlan {
    std::string snapshotId;
    TaskPlan taskPlan;
    bool reversible = true;
    std::string summary;
};

class BackupService {
public:
    explicit BackupService(std::filesystem::path root);

    SnapshotMetadata create_snapshot_manifest(const InstanceManifest& manifest, const std::string& label, std::string* error = nullptr);
    bool restore_snapshot(const SnapshotMetadata& snapshot, const std::filesystem::path& targetGameDir, std::string* error = nullptr) const;
    std::vector<SnapshotMetadata> list_snapshots(const std::string& instanceId, std::string* error = nullptr) const;
    RestorePlan build_restore_plan(const SnapshotMetadata& snapshot) const;

private:
    [[nodiscard]] std::filesystem::path snapshot_directory(const std::string& instanceId) const;
    [[nodiscard]] std::string make_snapshot_id(const std::string& instanceId) const;
    [[nodiscard]] std::string timestamp_now() const;

    std::filesystem::path root_;
};

} // namespace dawn::core
