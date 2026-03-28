#pragma once

#include "dawn/core/model/instance_manifest.h"
#include "dawn/core/model/task_types.h"

#include <filesystem>
#include <string>
#include <vector>
#include <functional>
#include <optional>

namespace dawn::core {

// Forward declarations
struct SnapshotMetadata;
struct RestorePlan;
struct SnapshotInfo;
struct AutoBackupConfig;
struct SnapshotCreateOptions;
struct SnapshotRestoreOptions;

// ============================================================================
// Data Structures
// ============================================================================

/// @brief Snapshot information structure
struct SnapshotInfo {
    std::string id;
    std::string instanceId;
    std::string name;
    std::string description;
    std::string createdAt;
    size_t size = 0;
    std::vector<std::string> includedPaths;
    std::filesystem::path archivePath;
    bool isAutoBackup = false;
};

/// @brief Auto backup configuration
struct AutoBackupConfig {
    bool enabled = false;
    bool backupBeforeLaunch = false;
    bool backupBeforeMajorUpdate = true;
    int scheduleType = 0; // 0=disabled, 1=daily, 2=weekly
    int retainCount = 5;
    std::string lastBackupAt;
    std::string nextScheduledAt;
};

/// @brief Snapshot creation options
struct SnapshotCreateOptions {
    std::string name;
    std::string description;
    bool includeMods = true;
    bool includeResourcePacks = true;
    bool includeShaderPacks = true;
    bool includeSaves = true;
    bool includeConfig = true;
    bool isAutoBackup = false;
};

/// @brief Snapshot restore options
struct SnapshotRestoreOptions {
    bool createRollbackPoint = true;
    std::string rollbackPointName;
};

/// @brief Backup operation progress callback
using BackupProgressCallback = std::function<void(const std::string& stage, int progress, const std::string& detail)>;

// ============================================================================
// Legacy Structures (backward compatible)
// ============================================================================

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

// ============================================================================
// BackupService Class
// ============================================================================

class BackupService {
public:
    explicit BackupService(std::filesystem::path root);

    // ------------------------------------------------------------------------
    // Legacy API (backward compatible)
    // ------------------------------------------------------------------------
    SnapshotMetadata create_snapshot_manifest(const InstanceManifest& manifest, const std::string& label, std::string* error = nullptr);
    bool restore_snapshot(const SnapshotMetadata& snapshot, const std::filesystem::path& targetGameDir, std::string* error = nullptr) const;
    std::vector<SnapshotMetadata> list_snapshots(const std::string& instanceId, std::string* error = nullptr) const;
    RestorePlan build_restore_plan(const SnapshotMetadata& snapshot) const;

    // ------------------------------------------------------------------------
    // New Snapshot API (Task 3.1)
    // ------------------------------------------------------------------------

    /// @brief Create instance snapshot (ZIP compressed format)
    /// @param instanceId Instance ID
    /// @param gameDir Game directory path
    /// @param options Snapshot creation options
    /// @param callback Progress callback (optional)
    /// @param error Error message output
    /// @return Created snapshot info, std::nullopt on failure
    std::optional<SnapshotInfo> create_snapshot(
        const std::string& instanceId,
        const std::filesystem::path& gameDir,
        const SnapshotCreateOptions& options,
        BackupProgressCallback callback = nullptr,
        std::string* error = nullptr);

    /// @brief Restore instance from snapshot
    /// @param instanceId Instance ID
    /// @param snapshotId Snapshot ID
    /// @param gameDir Target game directory
    /// @param options Restore options
    /// @param callback Progress callback (optional)
    /// @param error Error message output
    /// @return true if restore successful
    bool restore_snapshot(
        const std::string& instanceId,
        const std::string& snapshotId,
        const std::filesystem::path& gameDir,
        const SnapshotRestoreOptions& options = {},
        BackupProgressCallback callback = nullptr,
        std::string* error = nullptr) const;

    /// @brief Get snapshot information
    /// @param instanceId Instance ID
    /// @param snapshotId Snapshot ID
    /// @param error Error message output
    /// @return Snapshot info, std::nullopt if not found
    std::optional<SnapshotInfo> get_snapshot_info(
        const std::string& instanceId,
        const std::string& snapshotId,
        std::string* error = nullptr) const;

    // ------------------------------------------------------------------------
    // Snapshot Management API
    // ------------------------------------------------------------------------

    /// @brief List all snapshots for an instance
    /// @param instanceId Instance ID
    /// @param error Error message output
    /// @return List of snapshots
    std::vector<SnapshotInfo> list_snapshots_info(
        const std::string& instanceId,
        std::string* error = nullptr) const;

    /// @brief Delete a snapshot
    /// @param instanceId Instance ID
    /// @param snapshotId Snapshot ID
    /// @param error Error message output
    /// @return true if deletion successful
    bool delete_snapshot(
        const std::string& instanceId,
        const std::string& snapshotId,
        std::string* error = nullptr) const;

    /// @brief Export snapshot to specified path
    /// @param instanceId Instance ID
    /// @param snapshotId Snapshot ID
    /// @param outputPath Output file path
    /// @param callback Progress callback (optional)
    /// @param error Error message output
    /// @return true if export successful
    bool export_snapshot(
        const std::string& instanceId,
        const std::string& snapshotId,
        const std::filesystem::path& outputPath,
        BackupProgressCallback callback = nullptr,
        std::string* error = nullptr) const;

    /// @brief Import a snapshot
    /// @param instanceId Instance ID
    /// @param importPath Path to import ZIP file
    /// @param error Error message output
    /// @return Imported snapshot info, std::nullopt on failure
    std::optional<SnapshotInfo> import_snapshot(
        const std::string& instanceId,
        const std::filesystem::path& importPath,
        std::string* error = nullptr);

    // ------------------------------------------------------------------------
    // Auto Backup API
    // ------------------------------------------------------------------------

    /// @brief Configure auto backup for an instance
    /// @param instanceId Instance ID
    /// @param config Auto backup configuration
    /// @param error Error message output
    /// @return true if configuration successful
    bool configure_auto_backup(
        const std::string& instanceId,
        const AutoBackupConfig& config,
        std::string* error = nullptr);

    /// @brief Get auto backup configuration
    /// @param instanceId Instance ID
    /// @param error Error message output
    /// @return Auto backup configuration
    AutoBackupConfig get_auto_backup_config(
        const std::string& instanceId,
        std::string* error = nullptr) const;

    /// @brief Check and run scheduled backup (should be called by scheduler)
    /// @param instanceId Instance ID
    /// @param gameDir Game directory
    /// @param callback Progress callback (optional)
    /// @param error Error message output
    /// @return Created snapshot info if backup was performed
    std::optional<SnapshotInfo> check_and_run_scheduled_backup(
        const std::string& instanceId,
        const std::filesystem::path& gameDir,
        BackupProgressCallback callback = nullptr,
        std::string* error = nullptr);

    /// @brief Auto backup before launch (if configured)
    /// @param instanceId Instance ID
    /// @param gameDir Game directory
    /// @param callback Progress callback (optional)
    /// @param error Error message output
    /// @return Created snapshot info if backup was performed
    std::optional<SnapshotInfo> auto_backup_before_launch(
        const std::string& instanceId,
        const std::filesystem::path& gameDir,
        BackupProgressCallback callback = nullptr,
        std::string* error = nullptr);

    /// @brief Auto backup before major update (if configured)
    /// @param instanceId Instance ID
    /// @param gameDir Game directory
    /// @param callback Progress callback (optional)
    /// @param error Error message output
    /// @return Created snapshot info if backup was performed
    std::optional<SnapshotInfo> auto_backup_before_major_update(
        const std::string& instanceId,
        const std::filesystem::path& gameDir,
        BackupProgressCallback callback = nullptr,
        std::string* error = nullptr);

    /// @brief Clean up old snapshots (according to retention policy)
    /// @param instanceId Instance ID
    /// @param error Error message output
    /// @return Number of deleted snapshots
    int cleanup_old_snapshots(
        const std::string& instanceId,
        std::string* error = nullptr) const;

    /// @brief Verify snapshot integrity
    /// @param instanceId Instance ID
    /// @param snapshotId Snapshot ID
    /// @param error Error message output
    /// @return true if verification passed
    bool verify_snapshot_integrity(
        const std::string& instanceId,
        const std::string& snapshotId,
        std::string* error = nullptr) const;

private:
    // ------------------------------------------------------------------------
    // Helper Methods
    // ------------------------------------------------------------------------

    std::filesystem::path snapshot_directory(const std::string& instanceId) const;
    std::filesystem::path auto_backup_config_path(const std::string& instanceId) const;
    std::string make_snapshot_id(const std::string& instanceId) const;
    std::string timestamp_now() const;
    std::string timestamp_for_filename() const;

    bool save_snapshot_info(const SnapshotInfo& info, std::string* error = nullptr) const;
    std::optional<SnapshotInfo> load_snapshot_info(const std::filesystem::path& path, std::string* error = nullptr) const;

    std::vector<std::filesystem::path> collect_snapshot_paths(
        const std::filesystem::path& gameDir,
        const SnapshotCreateOptions& options) const;

    bool create_zip_archive(
        const std::vector<std::filesystem::path>& sourcePaths,
        const std::filesystem::path& baseDir,
        const std::filesystem::path& zipPath,
        BackupProgressCallback callback,
        std::string* error = nullptr) const;

    bool extract_zip_archive(
        const std::filesystem::path& zipPath,
        const std::filesystem::path& targetDir,
        BackupProgressCallback callback,
        std::string* error = nullptr) const;

    bool copy_directory_contents(
        const std::filesystem::path& source,
        const std::filesystem::path& target,
        std::string* error = nullptr) const;

    std::optional<SnapshotInfo> create_rollback_point(
        const std::string& instanceId,
        const std::filesystem::path& gameDir,
        const std::string& reason,
        BackupProgressCallback callback,
        std::string* error = nullptr) const;

    std::filesystem::path root_;
};

} // namespace dawn::core
