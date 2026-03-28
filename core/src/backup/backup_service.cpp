#include "dawn/core/backup/backup_service.h"

#include "dawn/infra/archive/zip_archive.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;

// ============================================================================
// JSON Serialization Helpers
// ============================================================================

Value snapshot_info_to_json(const SnapshotInfo& info) {
    Value::Object object;
    object.emplace("id", info.id);
    object.emplace("instanceId", info.instanceId);
    object.emplace("name", info.name);
    object.emplace("description", info.description);
    object.emplace("createdAt", info.createdAt);
    object.emplace("size", static_cast<double>(info.size));
    
    Value::Array paths;
    for (const auto& path : info.includedPaths) {
        paths.push_back(Value(path));
    }
    object.emplace("includedPaths", Value(std::move(paths)));
    object.emplace("archivePath", info.archivePath.generic_string());
    object.emplace("isAutoBackup", info.isAutoBackup);
    return Value(std::move(object));
}

std::optional<SnapshotInfo> snapshot_info_from_json(const Value& value) {
    if (!value.is_object()) {
        return std::nullopt;
    }
    
    SnapshotInfo info;
    const auto& object = value.as_object();
    
    if (const auto* entry = dawn::infra::json::find(object, "id"); entry && entry->is_string()) {
        info.id = entry->as_string();
    }
    if (const auto* entry = dawn::infra::json::find(object, "instanceId"); entry && entry->is_string()) {
        info.instanceId = entry->as_string();
    }
    if (const auto* entry = dawn::infra::json::find(object, "name"); entry && entry->is_string()) {
        info.name = entry->as_string();
    }
    if (const auto* entry = dawn::infra::json::find(object, "description"); entry && entry->is_string()) {
        info.description = entry->as_string();
    }
    if (const auto* entry = dawn::infra::json::find(object, "createdAt"); entry && entry->is_string()) {
        info.createdAt = entry->as_string();
    }
    if (const auto* entry = dawn::infra::json::find(object, "size"); entry && entry->is_number()) {
        info.size = static_cast<size_t>(entry->as_number());
    }
    if (const auto* entry = dawn::infra::json::find(object, "includedPaths"); entry && entry->is_array()) {
        for (const auto& path : entry->as_array()) {
            if (path.is_string()) {
                info.includedPaths.push_back(path.as_string());
            }
        }
    }
    if (const auto* entry = dawn::infra::json::find(object, "archivePath"); entry && entry->is_string()) {
        info.archivePath = std::filesystem::path(entry->as_string());
    }
    if (const auto* entry = dawn::infra::json::find(object, "isAutoBackup"); entry && entry->is_bool()) {
        info.isAutoBackup = entry->as_bool();
    }
    
    return info;
}

Value auto_backup_config_to_json(const AutoBackupConfig& config) {
    Value::Object object;
    object.emplace("enabled", config.enabled);
    object.emplace("backupBeforeLaunch", config.backupBeforeLaunch);
    object.emplace("backupBeforeMajorUpdate", config.backupBeforeMajorUpdate);
    object.emplace("scheduleType", config.scheduleType);
    object.emplace("retainCount", config.retainCount);
    object.emplace("lastBackupAt", config.lastBackupAt);
    object.emplace("nextScheduledAt", config.nextScheduledAt);
    return Value(std::move(object));
}

AutoBackupConfig auto_backup_config_from_json(const Value& value) {
    AutoBackupConfig config;
    if (!value.is_object()) {
        return config;
    }
    
    const auto& object = value.as_object();
    if (const auto* entry = dawn::infra::json::find(object, "enabled"); entry && entry->is_bool()) {
        config.enabled = entry->as_bool();
    }
    if (const auto* entry = dawn::infra::json::find(object, "backupBeforeLaunch"); entry && entry->is_bool()) {
        config.backupBeforeLaunch = entry->as_bool();
    }
    if (const auto* entry = dawn::infra::json::find(object, "backupBeforeMajorUpdate"); entry && entry->is_bool()) {
        config.backupBeforeMajorUpdate = entry->as_bool();
    }
    if (const auto* entry = dawn::infra::json::find(object, "scheduleType"); entry && entry->is_number()) {
        config.scheduleType = static_cast<int>(entry->as_number());
    }
    if (const auto* entry = dawn::infra::json::find(object, "retainCount"); entry && entry->is_number()) {
        config.retainCount = static_cast<int>(entry->as_number());
    }
    if (const auto* entry = dawn::infra::json::find(object, "lastBackupAt"); entry && entry->is_string()) {
        config.lastBackupAt = entry->as_string();
    }
    if (const auto* entry = dawn::infra::json::find(object, "nextScheduledAt"); entry && entry->is_string()) {
        config.nextScheduledAt = entry->as_string();
    }
    
    return config;
}

// Legacy JSON helpers (保持向后兼容)
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

// ============================================================================
// Helper Functions
// ============================================================================

std::string make_timestamp_filename() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    // Get milliseconds for uniqueness
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    std::ostringstream out;
    out << std::put_time(&tm, "%Y%m%d_%H%M%S") << '_' << std::setfill('0') << std::setw(3) << ms.count();
    return out.str();
}

std::string make_iso_timestamp() {
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

bool should_run_scheduled_backup(const AutoBackupConfig& config) {
    if (!config.enabled || config.scheduleType == 0) {
        return false;
    }
    
    if (config.lastBackupAt.empty()) {
        return true;
    }
    
    // Parse last backup time
    std::tm lastTm = {};
    std::istringstream lastStream(config.lastBackupAt);
    lastStream >> std::get_time(&lastTm, "%Y-%m-%dT%H:%M:%S");
    if (lastStream.fail()) {
        return true;
    }
    
    auto lastTime = std::mktime(&lastTm);
    auto now = std::time(nullptr);
    
    double diffHours = std::difftime(now, lastTime) / 3600.0;
    
    if (config.scheduleType == 1) {  // Daily
        return diffHours >= 24;
    } else if (config.scheduleType == 2) {  // Weekly
        return diffHours >= 24 * 7;
    }
    
    return false;
}

std::string calculate_next_backup_time(const AutoBackupConfig& config) {
    if (!config.enabled || config.scheduleType == 0) {
        return "";
    }
    
    auto now = std::chrono::system_clock::now();
    std::time_t nextTime;
    
    if (config.scheduleType == 1) {  // Daily
        nextTime = std::chrono::system_clock::to_time_t(now + std::chrono::hours(24));
    } else {  // Weekly
        nextTime = std::chrono::system_clock::to_time_t(now + std::chrono::hours(24 * 7));
    }
    
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &nextTime);
#else
    localtime_r(&nextTime, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

} // anonymous namespace

// ============================================================================
// Constructor
// ============================================================================

BackupService::BackupService(std::filesystem::path root) : root_(std::move(root)) {}

// ============================================================================
// Legacy API Implementation (保持向后兼容)
// ============================================================================

SnapshotMetadata BackupService::create_snapshot_manifest(const InstanceManifest& manifest, 
                                                          const std::string& label, 
                                                          std::string* error) {
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

bool BackupService::restore_snapshot(const SnapshotMetadata& snapshot, 
                                      const std::filesystem::path& targetGameDir, 
                                      std::string* error) const {
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

std::vector<SnapshotMetadata> BackupService::list_snapshots(const std::string& instanceId, 
                                                             std::string* error) const {
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

// ============================================================================
// New Snapshot API Implementation (任务 3.1)
// ============================================================================

std::optional<SnapshotInfo> BackupService::create_snapshot(
    const std::string& instanceId,
    const std::filesystem::path& gameDir,
    const SnapshotCreateOptions& options,
    BackupProgressCallback callback,
    std::string* error) {
    
    std::error_code ec;
    
    // Validate game directory
    if (!std::filesystem::exists(gameDir, ec)) {
        if (error) *error = "Game directory does not exist: " + gameDir.string();
        return std::nullopt;
    }
    
    // Generate snapshot ID and paths
    std::string timestamp = make_timestamp_filename();
    std::string snapshotId = instanceId + "_" + timestamp;
    std::string safeName = options.name.empty() ? "snapshot" : options.name;
    
    // Replace invalid filename characters
    std::replace_if(safeName.begin(), safeName.end(), 
        [](char c) { return c == '\\' || c == '/' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|'; }, '_');
    
    auto snapshotDir = snapshot_directory(instanceId);
    std::filesystem::create_directories(snapshotDir, ec);
    if (ec) {
        if (error) *error = "Failed to create snapshot directory: " + ec.message();
        return std::nullopt;
    }
    
    std::string zipFilename = timestamp + "_" + safeName + ".zip";
    auto zipPath = snapshotDir / zipFilename;
    auto metadataPath = snapshotDir / (snapshotId + ".json");
    
    // Collect paths to include in snapshot
    auto pathsToInclude = collect_snapshot_paths(gameDir, options);
    if (pathsToInclude.empty()) {
        if (error) *error = "No files found to include in snapshot";
        return std::nullopt;
    }
    
    // Report progress
    if (callback) callback("collecting", 10, "Collected " + std::to_string(pathsToInclude.size()) + " paths");
    
    // Create ZIP archive
    auto zipCallback = [&callback](const std::filesystem::path& currentFile, 
                                    std::uint64_t processed, 
                                    std::uint64_t total) -> bool {
        if (callback && total > 0) {
            int progress = static_cast<int>(10 + (processed * 80 / total));
            callback("compressing", progress, "Compressing: " + currentFile.filename().string());
        }
        return true;
    };
    
    if (!dawn::infra::archive::create_zip_archive(pathsToInclude, gameDir, zipPath, zipCallback, error)) {
        // Clean up partial file
        std::filesystem::remove(zipPath, ec);
        return std::nullopt;
    }
    
    if (callback) callback("finalizing", 95, "Creating metadata");
    
    // Get ZIP file size
    auto zipSize = dawn::infra::fs::file_size(zipPath, error);
    
    // Build included paths list
    std::vector<std::string> includedPaths;
    for (const auto& path : pathsToInclude) {
        auto rel = std::filesystem::relative(path, gameDir, ec);
        if (!ec) {
            includedPaths.push_back(rel.generic_string());
        }
    }
    
    // Create snapshot info
    SnapshotInfo info;
    info.id = snapshotId;
    info.instanceId = instanceId;
    info.name = options.name.empty() ? "Snapshot " + timestamp : options.name;
    info.description = options.description;
    info.createdAt = make_iso_timestamp();
    info.size = zipSize;
    info.includedPaths = std::move(includedPaths);
    info.archivePath = zipPath;
    info.isAutoBackup = options.isAutoBackup;
    
    // Save metadata
    if (!save_snapshot_info(info, error)) {
        std::filesystem::remove(zipPath, ec);
        return std::nullopt;
    }
    
    if (callback) callback("complete", 100, "Snapshot created successfully");
    
    return info;
}

bool BackupService::restore_snapshot(
    const std::string& instanceId,
    const std::string& snapshotId,
    const std::filesystem::path& gameDir,
    const SnapshotRestoreOptions& options,
    BackupProgressCallback callback,
    std::string* error) const {
    
    std::error_code ec;
    
    // Get snapshot info
    auto infoOpt = get_snapshot_info(instanceId, snapshotId, error);
    if (!infoOpt) {
        if (error && error->empty()) *error = "Snapshot not found: " + snapshotId;
        return false;
    }
    
    const auto& info = *infoOpt;
    
    if (callback) callback("validating", 5, "Validating snapshot");
    
    // Verify ZIP integrity
    if (!dawn::infra::archive::verify_zip_archive(info.archivePath, error)) {
        return false;
    }
    
    // Create rollback point if requested
    if (options.createRollbackPoint) {
        if (callback) callback("rollback", 10, "Creating rollback point");
        
        std::string rollbackName = options.rollbackPointName.empty() 
            ? "auto-rollback-before-" + snapshotId 
            : options.rollbackPointName;
        
        SnapshotCreateOptions rollbackOptions;
        rollbackOptions.name = rollbackName;
        rollbackOptions.description = "Automatic rollback point before restoring " + snapshotId;
        rollbackOptions.includeMods = true;
        rollbackOptions.includeResourcePacks = true;
        rollbackOptions.includeShaderPacks = true;
        rollbackOptions.includeSaves = true;
        rollbackOptions.includeConfig = true;
        rollbackOptions.isAutoBackup = true;
        
        auto rollbackCallback = [&callback](const std::string& stage, int progress, const std::string& detail) {
            if (callback) callback("rollback", 10 + progress / 5, detail);
        };
        
        auto rollbackResult = const_cast<BackupService*>(this)->create_rollback_point(
            instanceId, gameDir, "restore", rollbackCallback, error);
        
        if (!rollbackResult) {
            if (error && error->empty()) *error = "Failed to create rollback point";
            return false;
        }
    }
    
    if (callback) callback("extracting", 30, "Extracting snapshot files");
    
    // Create temp extraction directory
    auto tempDir = root_ / "temp" / "restore" / instanceId / snapshotId;
    std::filesystem::remove_all(tempDir, ec);
    std::filesystem::create_directories(tempDir, ec);
    
    // Extract ZIP
    auto extractCallback = [&callback](const std::filesystem::path& currentFile,
                                        std::uint64_t processed,
                                        std::uint64_t total) -> bool {
        if (callback && total > 0) {
            int progress = static_cast<int>(30 + (processed * 50 / total));
            callback("extracting", progress, "Extracting: " + currentFile.filename().string());
        }
        return true;
    };
    
    if (!dawn::infra::archive::extract_zip_archive(info.archivePath, tempDir, extractCallback, error)) {
        std::filesystem::remove_all(tempDir, ec);
        return false;
    }
    
    if (callback) callback("installing", 85, "Installing files to game directory");
    
    // Copy extracted files to game directory
    if (!copy_directory_contents(tempDir, gameDir, error)) {
        std::filesystem::remove_all(tempDir, ec);
        return false;
    }
    
    // Clean up temp directory
    std::filesystem::remove_all(tempDir, ec);
    
    if (callback) callback("verifying", 95, "Verifying restored files");
    
    // Verify restored files
    bool verified = true;
    for (const auto& includedPath : info.includedPaths) {
        auto fullPath = gameDir / includedPath;
        if (!std::filesystem::exists(fullPath, ec)) {
            verified = false;
            if (error) *error = "Verification failed: missing " + includedPath;
            break;
        }
    }
    
    if (!verified) {
        return false;
    }
    
    if (callback) callback("complete", 100, "Restore completed successfully");
    
    return true;
}

std::optional<SnapshotInfo> BackupService::get_snapshot_info(
    const std::string& instanceId,
    const std::string& snapshotId,
    std::string* error) const {
    
    auto metadataPath = snapshot_directory(instanceId) / (snapshotId + ".json");
    return load_snapshot_info(metadataPath, error);
}

// ============================================================================
// Snapshot Management API Implementation
// ============================================================================

std::vector<SnapshotInfo> BackupService::list_snapshots_info(
    const std::string& instanceId,
    std::string* error) const {
    
    std::vector<SnapshotInfo> result;
    auto dir = snapshot_directory(instanceId);
    
    if (!std::filesystem::exists(dir)) {
        return result;
    }
    
    for (const auto& path : dawn::infra::fs::list_files(dir, ".json")) {
        auto info = load_snapshot_info(path, error);
        if (info) {
            result.push_back(std::move(*info));
        }
    }
    
    // Sort by creation time (newest first)
    std::sort(result.begin(), result.end(), [](const SnapshotInfo& a, const SnapshotInfo& b) {
        return a.createdAt > b.createdAt;
    });
    
    return result;
}

bool BackupService::delete_snapshot(
    const std::string& instanceId,
    const std::string& snapshotId,
    std::string* error) const {
    
    std::error_code ec;
    auto snapshotDir = snapshot_directory(instanceId);
    
    // Delete metadata file
    auto metadataPath = snapshotDir / (snapshotId + ".json");
    if (std::filesystem::exists(metadataPath, ec)) {
        std::filesystem::remove(metadataPath, ec);
        if (ec) {
            if (error) *error = "Failed to delete metadata: " + ec.message();
            return false;
        }
    }
    
    // Delete ZIP file
    // Try to find ZIP file with matching snapshot ID pattern
    for (const auto& entry : std::filesystem::directory_iterator(snapshotDir, ec)) {
        if (entry.is_regular_file() && entry.path().extension() == ".zip") {
            auto filename = entry.path().filename().string();
            // Check if filename starts with timestamp from snapshotId
            if (snapshotId.find('_') != std::string::npos) {
                auto timestamp = snapshotId.substr(snapshotId.find('_') + 1);
                if (filename.find(timestamp) == 0) {
                    std::filesystem::remove(entry.path(), ec);
                    break;
                }
            }
        }
    }
    
    return true;
}

bool BackupService::export_snapshot(
    const std::string& instanceId,
    const std::string& snapshotId,
    const std::filesystem::path& outputPath,
    BackupProgressCallback callback,
    std::string* error) const {
    
    std::error_code ec;
    
    auto infoOpt = get_snapshot_info(instanceId, snapshotId, error);
    if (!infoOpt) {
        if (error && error->empty()) *error = "Snapshot not found";
        return false;
    }
    
    const auto& info = *infoOpt;
    
    if (!std::filesystem::exists(info.archivePath, ec)) {
        if (error) *error = "Snapshot archive not found";
        return false;
    }
    
    // Ensure output directory exists
    std::filesystem::create_directories(outputPath.parent_path(), ec);
    if (ec) {
        if (error) *error = "Failed to create output directory: " + ec.message();
        return false;
    }
    
    if (callback) callback("exporting", 0, "Starting export");
    
    // Copy the ZIP file
    std::filesystem::copy_file(info.archivePath, outputPath, 
                                std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error) *error = "Failed to export snapshot: " + ec.message();
        return false;
    }
    
    if (callback) callback("complete", 100, "Export completed");
    
    return true;
}

std::optional<SnapshotInfo> BackupService::import_snapshot(
    const std::string& instanceId,
    const std::filesystem::path& importPath,
    std::string* error) {
    
    std::error_code ec;
    
    if (!std::filesystem::exists(importPath, ec)) {
        if (error) *error = "Import file does not exist";
        return std::nullopt;
    }
    
    if (!dawn::infra::archive::verify_zip_archive(importPath, error)) {
        return std::nullopt;
    }
    
    auto snapshotDir = snapshot_directory(instanceId);
    std::filesystem::create_directories(snapshotDir, ec);
    
    // Generate new snapshot ID
    std::string timestamp = make_timestamp_filename();
    std::string snapshotId = instanceId + "_" + timestamp + "_imported";
    
    std::string zipFilename = timestamp + "_imported.zip";
    auto destZipPath = snapshotDir / zipFilename;
    auto metadataPath = snapshotDir / (snapshotId + ".json");
    
    // Copy ZIP file
    std::filesystem::copy_file(importPath, destZipPath, 
                                std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        if (error) *error = "Failed to copy snapshot: " + ec.message();
        return std::nullopt;
    }
    
    // Get ZIP info
    std::size_t totalFiles = 0;
    std::uint64_t totalSize = 0;
    std::uint64_t compressedSize = 0;
    dawn::infra::archive::get_zip_info(destZipPath, totalFiles, totalSize, compressedSize, error);
    
    // Create snapshot info
    SnapshotInfo info;
    info.id = snapshotId;
    info.instanceId = instanceId;
    info.name = "Imported Snapshot";
    info.description = "Imported from " + importPath.filename().string();
    info.createdAt = make_iso_timestamp();
    info.size = compressedSize;
    info.archivePath = destZipPath;
    info.isAutoBackup = false;
    
    // Save metadata
    if (!save_snapshot_info(info, error)) {
        std::filesystem::remove(destZipPath, ec);
        return std::nullopt;
    }
    
    return info;
}

// ============================================================================
// Auto Backup API Implementation
// ============================================================================

bool BackupService::configure_auto_backup(
    const std::string& instanceId,
    const AutoBackupConfig& config,
    std::string* error) {
    
    auto configPath = auto_backup_config_path(instanceId);
    std::error_code ec;
    std::filesystem::create_directories(configPath.parent_path(), ec);
    if (ec) {
        if (error) *error = "Failed to create config directory: " + ec.message();
        return false;
    }
    
    // Calculate next scheduled backup time
    auto configCopy = config;
    if (config.enabled && config.scheduleType > 0) {
        configCopy.nextScheduledAt = calculate_next_backup_time(config);
    }
    
    auto json = auto_backup_config_to_json(configCopy);
    return dawn::infra::fs::write_text_file(configPath, 
                                             dawn::infra::json::stringify(json, 2), 
                                             error);
}

AutoBackupConfig BackupService::get_auto_backup_config(
    const std::string& instanceId,
    std::string* error) const {
    
    auto configPath = auto_backup_config_path(instanceId);
    std::string text;
    
    if (!dawn::infra::fs::read_text_file(configPath, &text, error)) {
        // Return default config if file doesn't exist
        return AutoBackupConfig{};
    }
    
    auto parsed = dawn::infra::json::parse(text);
    if (!parsed.ok) {
        if (error) *error = parsed.error.message;
        return AutoBackupConfig{};
    }
    
    return auto_backup_config_from_json(parsed.value);
}

std::optional<SnapshotInfo> BackupService::check_and_run_scheduled_backup(
    const std::string& instanceId,
    const std::filesystem::path& gameDir,
    BackupProgressCallback callback,
    std::string* error) {
    
    auto config = get_auto_backup_config(instanceId, error);
    
    if (!config.enabled || config.scheduleType == 0) {
        return std::nullopt;
    }
    
    if (!should_run_scheduled_backup(config)) {
        return std::nullopt;
    }
    
    // Create scheduled backup
    SnapshotCreateOptions options;
    options.name = "Scheduled Backup";
    options.description = "Automatic scheduled backup";
    options.includeMods = true;
    options.includeResourcePacks = true;
    options.includeShaderPacks = true;
    options.includeSaves = true;
    options.includeConfig = true;
    options.isAutoBackup = true;
    
    auto result = create_snapshot(instanceId, gameDir, options, callback, error);
    
    if (result) {
        // Update config with last backup time
        config.lastBackupAt = make_iso_timestamp();
        config.nextScheduledAt = calculate_next_backup_time(config);
        configure_auto_backup(instanceId, config, nullptr);
        
        // Clean up old snapshots
        cleanup_old_snapshots(instanceId, nullptr);
    }
    
    return result;
}

std::optional<SnapshotInfo> BackupService::auto_backup_before_launch(
    const std::string& instanceId,
    const std::filesystem::path& gameDir,
    BackupProgressCallback callback,
    std::string* error) {
    
    auto config = get_auto_backup_config(instanceId, error);
    
    if (!config.enabled || !config.backupBeforeLaunch) {
        return std::nullopt;
    }
    
    SnapshotCreateOptions options;
    options.name = "Pre-launch Backup";
    options.description = "Automatic backup before game launch";
    options.includeMods = true;
    options.includeResourcePacks = true;
    options.includeShaderPacks = true;
    options.includeSaves = true;
    options.includeConfig = true;
    options.isAutoBackup = true;
    
    auto result = create_snapshot(instanceId, gameDir, options, callback, error);
    
    if (result) {
        // Update config with last backup time
        config.lastBackupAt = make_iso_timestamp();
        configure_auto_backup(instanceId, config, nullptr);
        
        // Clean up old snapshots
        cleanup_old_snapshots(instanceId, nullptr);
    }
    
    return result;
}

std::optional<SnapshotInfo> BackupService::auto_backup_before_major_update(
    const std::string& instanceId,
    const std::filesystem::path& gameDir,
    BackupProgressCallback callback,
    std::string* error) {
    
    auto config = get_auto_backup_config(instanceId, error);
    
    if (!config.enabled || !config.backupBeforeMajorUpdate) {
        return std::nullopt;
    }
    
    SnapshotCreateOptions options;
    options.name = "Pre-update Backup";
    options.description = "Automatic backup before major update";
    options.includeMods = true;
    options.includeResourcePacks = true;
    options.includeShaderPacks = true;
    options.includeSaves = true;
    options.includeConfig = true;
    options.isAutoBackup = true;
    
    auto result = create_snapshot(instanceId, gameDir, options, callback, error);
    
    if (result) {
        // Update config with last backup time
        config.lastBackupAt = make_iso_timestamp();
        configure_auto_backup(instanceId, config, nullptr);
        
        // Clean up old snapshots
        cleanup_old_snapshots(instanceId, nullptr);
    }
    
    return result;
}

int BackupService::cleanup_old_snapshots(
    const std::string& instanceId,
    std::string* error) const {
    
    auto config = get_auto_backup_config(instanceId, error);
    if (config.retainCount <= 0) {
        return 0;
    }
    
    auto snapshots = list_snapshots_info(instanceId, error);
    if (snapshots.size() <= static_cast<size_t>(config.retainCount)) {
        return 0;
    }
    
    int deletedCount = 0;
    // Delete oldest snapshots beyond retain count
    for (size_t i = config.retainCount; i < snapshots.size(); ++i) {
        if (delete_snapshot(instanceId, snapshots[i].id, nullptr)) {
            ++deletedCount;
        }
    }
    
    return deletedCount;
}

bool BackupService::verify_snapshot_integrity(
    const std::string& instanceId,
    const std::string& snapshotId,
    std::string* error) const {
    
    auto infoOpt = get_snapshot_info(instanceId, snapshotId, error);
    if (!infoOpt) {
        if (error && error->empty()) *error = "Snapshot not found";
        return false;
    }
    
    const auto& info = *infoOpt;
    
    // Check if archive exists
    std::error_code ec;
    if (!std::filesystem::exists(info.archivePath, ec)) {
        if (error) *error = "Snapshot archive not found";
        return false;
    }
    
    // Verify ZIP integrity
    if (!dawn::infra::archive::verify_zip_archive(info.archivePath, error)) {
        return false;
    }
    
    // Verify file size matches
    auto actualSize = dawn::infra::fs::file_size(info.archivePath, error);
    if (actualSize != info.size) {
        if (error) *error = "Snapshot file size mismatch";
        return false;
    }
    
    return true;
}

// ============================================================================
// Private Helper Methods
// ============================================================================

std::filesystem::path BackupService::snapshot_directory(const std::string& instanceId) const {
    return root_ / "backups" / "snapshots" / instanceId;
}

std::filesystem::path BackupService::auto_backup_config_path(const std::string& instanceId) const {
    return root_ / "backups" / "config" / (instanceId + "_auto_backup.json");
}

std::string BackupService::make_snapshot_id(const std::string& instanceId) const {
    const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    return instanceId + "-snapshot-" + std::to_string(millis);
}

std::string BackupService::timestamp_now() const {
    return make_iso_timestamp();
}

std::string BackupService::timestamp_for_filename() const {
    return make_timestamp_filename();
}

bool BackupService::save_snapshot_info(const SnapshotInfo& info, std::string* error) const {
    auto metadataPath = snapshot_directory(info.instanceId) / (info.id + ".json");
    auto json = snapshot_info_to_json(info);
    return dawn::infra::fs::write_text_file(metadataPath, 
                                             dawn::infra::json::stringify(json, 2), 
                                             error);
}

std::optional<SnapshotInfo> BackupService::load_snapshot_info(const std::filesystem::path& path, 
                                                               std::string* error) const {
    std::string text;
    if (!dawn::infra::fs::read_text_file(path, &text, error)) {
        return std::nullopt;
    }
    
    auto parsed = dawn::infra::json::parse(text);
    if (!parsed.ok) {
        if (error) *error = parsed.error.message;
        return std::nullopt;
    }
    
    return snapshot_info_from_json(parsed.value);
}

std::vector<std::filesystem::path> BackupService::collect_snapshot_paths(
    const std::filesystem::path& gameDir,
    const SnapshotCreateOptions& options) const {
    
    std::vector<std::filesystem::path> result;
    std::error_code ec;
    
    if (!std::filesystem::exists(gameDir, ec)) {
        return result;
    }
    
    // Always include instance_manifest.json if it exists
    auto manifestPath = gameDir / "instance_manifest.json";
    if (std::filesystem::exists(manifestPath, ec)) {
        result.push_back(manifestPath);
    }
    
    // Include mods directory
    if (options.includeMods) {
        auto modsDir = gameDir / "mods";
        if (std::filesystem::exists(modsDir, ec)) {
            result.push_back(modsDir);
        }
    }
    
    // Include resourcepacks directory
    if (options.includeResourcePacks) {
        auto resourcePacksDir = gameDir / "resourcepacks";
        if (std::filesystem::exists(resourcePacksDir, ec)) {
            result.push_back(resourcePacksDir);
        }
    }
    
    // Include shaderpacks directory
    if (options.includeShaderPacks) {
        auto shaderPacksDir = gameDir / "shaderpacks";
        if (std::filesystem::exists(shaderPacksDir, ec)) {
            result.push_back(shaderPacksDir);
        }
    }
    
    // Include saves directory
    if (options.includeSaves) {
        auto savesDir = gameDir / "saves";
        if (std::filesystem::exists(savesDir, ec)) {
            result.push_back(savesDir);
        }
    }
    
    // Include config directory
    if (options.includeConfig) {
        auto configDir = gameDir / "config";
        if (std::filesystem::exists(configDir, ec)) {
            result.push_back(configDir);
        }
        
        // Also include options.txt at root level
        auto optionsPath = gameDir / "options.txt";
        if (std::filesystem::exists(optionsPath, ec)) {
            result.push_back(optionsPath);
        }
    }
    
    return result;
}

bool BackupService::create_zip_archive(
    const std::vector<std::filesystem::path>& sourcePaths,
    const std::filesystem::path& baseDir,
    const std::filesystem::path& zipPath,
    BackupProgressCallback callback,
    std::string* error) const {
    
    auto zipCallback = [&callback](const std::filesystem::path& currentFile,
                                    std::uint64_t processed,
                                    std::uint64_t total) -> bool {
        if (callback) {
            int progress = static_cast<int>(processed * 100 / (total > 0 ? total : 1));
            callback("compressing", progress, "Compressing: " + currentFile.filename().string());
        }
        return true;
    };
    
    return dawn::infra::archive::create_zip_archive(sourcePaths, baseDir, zipPath, zipCallback, error);
}

bool BackupService::extract_zip_archive(
    const std::filesystem::path& zipPath,
    const std::filesystem::path& targetDir,
    BackupProgressCallback callback,
    std::string* error) const {
    
    auto zipCallback = [&callback](const std::filesystem::path& currentFile,
                                    std::uint64_t processed,
                                    std::uint64_t total) -> bool {
        if (callback) {
            int progress = static_cast<int>(processed * 100 / (total > 0 ? total : 1));
            callback("extracting", progress, "Extracting: " + currentFile.filename().string());
        }
        return true;
    };
    
    return dawn::infra::archive::extract_zip_archive(zipPath, targetDir, zipCallback, error);
}

bool BackupService::copy_directory_contents(
    const std::filesystem::path& source,
    const std::filesystem::path& target,
    std::string* error) const {
    
    std::error_code ec;
    
    if (!std::filesystem::exists(source, ec)) {
        if (error) *error = "Source directory does not exist";
        return false;
    }
    
    std::filesystem::create_directories(target, ec);
    if (ec) {
        if (error) *error = "Failed to create target directory: " + ec.message();
        return false;
    }
    
    for (const auto& entry : std::filesystem::recursive_directory_iterator(source, ec)) {
        if (ec) {
            if (error) *error = ec.message();
            return false;
        }
        
        const auto relative = std::filesystem::relative(entry.path(), source, ec);
        if (ec) {
            continue;
        }
        
        const auto destPath = target / relative;
        
        if (entry.is_directory()) {
            std::filesystem::create_directories(destPath, ec);
            if (ec) {
                if (error) *error = ec.message();
                return false;
            }
        } else {
            std::filesystem::create_directories(destPath.parent_path(), ec);
            if (ec) {
                if (error) *error = ec.message();
                return false;
            }
            
            std::filesystem::copy_file(entry.path(), destPath, 
                                        std::filesystem::copy_options::overwrite_existing, ec);
            if (ec) {
                if (error) *error = ec.message();
                return false;
            }
        }
    }
    
    return true;
}

std::optional<SnapshotInfo> BackupService::create_rollback_point(
    const std::string& instanceId,
    const std::filesystem::path& gameDir,
    const std::string& reason,
    BackupProgressCallback callback,
    std::string* error) const {
    
    SnapshotCreateOptions options;
    options.name = "Rollback-" + reason;
    options.description = "Automatic rollback point before " + reason;
    options.includeMods = true;
    options.includeResourcePacks = true;
    options.includeShaderPacks = true;
    options.includeSaves = true;
    options.includeConfig = true;
    options.isAutoBackup = true;
    
    return const_cast<BackupService*>(this)->create_snapshot(instanceId, gameDir, options, callback, error);
}

} // namespace dawn::core
