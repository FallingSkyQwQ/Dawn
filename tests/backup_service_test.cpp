#include "dawn/core/backup/backup_service.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <thread>

using namespace dawn::core;

// ============================================================================
// Legacy API Tests
// ============================================================================

TEST(BackupService, CreatesSnapshotManifestAndRestorePlan) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-backup-test";
    std::filesystem::remove_all(root);

    BackupService service(root);
    InstanceManifest manifest;
    manifest.id = "instance-01";
    manifest.name = "Test Instance";
    manifest.gameDir = (root / "instances" / manifest.id / "game").generic_string();

    std::filesystem::create_directories(std::filesystem::path(manifest.gameDir) / "config");
    std::filesystem::create_directories(std::filesystem::path(manifest.gameDir) / "saves");
    ASSERT_TRUE(static_cast<bool>(std::ofstream(std::filesystem::path(manifest.gameDir) / "config" / "options.txt")));
    ASSERT_TRUE(static_cast<bool>(std::ofstream(std::filesystem::path(manifest.gameDir) / "saves" / "world.dat")));

    std::string error;
    const auto snapshot = service.create_snapshot_manifest(manifest, "before update", &error);
    ASSERT_FALSE(snapshot.snapshotId.empty()) << error;
    EXPECT_EQ(snapshot.instanceId, "instance-01");
    EXPECT_TRUE(std::filesystem::exists(snapshot.archivePath / "game" / "config" / "options.txt"));

    const auto snapshots = service.list_snapshots("instance-01", &error);
    ASSERT_FALSE(snapshots.empty()) << error;
    EXPECT_EQ(snapshots.front().instanceId, "instance-01");

    const auto plan = service.build_restore_plan(snapshot);
    EXPECT_EQ(plan.snapshotId, snapshot.snapshotId);
    EXPECT_EQ(plan.taskPlan.steps.size(), 3u);
    EXPECT_TRUE(plan.reversible);

    const auto restoreTarget = root / "restore-target";
    ASSERT_TRUE(service.restore_snapshot(snapshot, restoreTarget, &error)) << error;
    EXPECT_TRUE(std::filesystem::exists(restoreTarget / "config" / "options.txt"));
    EXPECT_TRUE(std::filesystem::exists(restoreTarget / "saves" / "world.dat"));

    std::filesystem::remove_all(root);
}

// ============================================================================
// New Snapshot API Tests (Task 3.1)
// ============================================================================

class BackupServiceNewApiTest : public ::testing::Test {
protected:
    std::filesystem::path root;
    std::filesystem::path gameDir;
    std::unique_ptr<BackupService> service;
    std::string instanceId;
    static int testCounter;

    void SetUp() override {
        // Generate unique test directory for each test
        int testId = ++testCounter;
        root = std::filesystem::temp_directory_path() / ("dawn-backup-test-" + std::to_string(testId));
        std::filesystem::remove_all(root);
        
        instanceId = "test-instance-" + std::to_string(testId);
        gameDir = root / "instances" / instanceId / "game";
        std::filesystem::create_directories(gameDir / "mods");
        std::filesystem::create_directories(gameDir / "resourcepacks");
        std::filesystem::create_directories(gameDir / "shaderpacks");
        std::filesystem::create_directories(gameDir / "saves" / "world1");
        std::filesystem::create_directories(gameDir / "config");
        
        // Create test files
        std::ofstream(gameDir / "instance_manifest.json") << "{\"version\":\"1.0\"}";
        std::ofstream(gameDir / "mods" / "mod1.jar") << "mod content";
        std::ofstream(gameDir / "resourcepacks" / "pack1.zip") << "pack content";
        std::ofstream(gameDir / "shaderpacks" / "shader1.zip") << "shader content";
        std::ofstream(gameDir / "saves" / "world1" / "level.dat") << "world data";
        std::ofstream(gameDir / "config" / "options.txt") << "option=value";
        std::ofstream(gameDir / "options.txt") << "game options";
        
        service = std::make_unique<BackupService>(root);
    }

    void TearDown() override {
        std::filesystem::remove_all(root);
    }
};

int BackupServiceNewApiTest::testCounter = 0;

TEST_F(BackupServiceNewApiTest, CreateSnapshotWithZipCompression) {
    SnapshotCreateOptions options;
    options.name = "Test Snapshot";
    options.description = "Test snapshot description";
    options.includeMods = true;
    options.includeResourcePacks = true;
    options.includeShaderPacks = true;
    options.includeSaves = true;
    options.includeConfig = true;
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
    
    ASSERT_TRUE(snapshot.has_value()) << error;
    EXPECT_EQ(snapshot->instanceId, instanceId);
    EXPECT_EQ(snapshot->name, "Test Snapshot");
    EXPECT_EQ(snapshot->description, "Test snapshot description");
    EXPECT_FALSE(snapshot->id.empty());
    EXPECT_FALSE(snapshot->createdAt.empty());
    EXPECT_GT(snapshot->size, 0);
    EXPECT_FALSE(snapshot->isAutoBackup);
    
    // Verify ZIP file was created
    EXPECT_TRUE(std::filesystem::exists(snapshot->archivePath));
    EXPECT_EQ(snapshot->archivePath.extension(), ".zip");
    
    // Verify metadata file was created
    auto metadataPath = root / "backups" / "snapshots" / instanceId / (snapshot->id + ".json");
    EXPECT_TRUE(std::filesystem::exists(metadataPath));
}

TEST_F(BackupServiceNewApiTest, CreateSnapshotWithProgressCallback) {
    SnapshotCreateOptions options;
    options.name = "Progress Test";
    options.includeMods = true;
    options.includeSaves = true;
    
    std::vector<std::tuple<std::string, int, std::string>> progressEvents;
    
    auto callback = [&progressEvents](const std::string& stage, int progress, const std::string& detail) {
        progressEvents.emplace_back(stage, progress, detail);
    };
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, gameDir, options, callback, &error);
    
    ASSERT_TRUE(snapshot.has_value()) << error;
    EXPECT_FALSE(progressEvents.empty());
    
    // Check that we got progress events
    bool foundCollecting = false;
    bool foundCompressing = false;
    bool foundComplete = false;
    
    for (const auto& [stage, progress, detail] : progressEvents) {
        if (stage == "collecting") foundCollecting = true;
        if (stage == "compressing") foundCompressing = true;
        if (stage == "complete") foundComplete = true;
    }
    
    EXPECT_TRUE(foundCollecting);
    EXPECT_TRUE(foundCompressing);
    EXPECT_TRUE(foundComplete);
}

TEST_F(BackupServiceNewApiTest, GetSnapshotInfo) {
    // Create a snapshot first
    SnapshotCreateOptions options;
    options.name = "Info Test";
    
    std::string error;
    auto created = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
    ASSERT_TRUE(created.has_value()) << error;
    
    // Retrieve the snapshot info
    auto retrieved = service->get_snapshot_info(instanceId, created->id, &error);
    ASSERT_TRUE(retrieved.has_value()) << error;
    
    EXPECT_EQ(retrieved->id, created->id);
    EXPECT_EQ(retrieved->name, created->name);
    EXPECT_EQ(retrieved->instanceId, created->instanceId);
}

TEST_F(BackupServiceNewApiTest, ListSnapshotsInfo) {
    // Create multiple snapshots
    SnapshotCreateOptions options1;
    options1.name = "Snapshot 1";
    
    SnapshotCreateOptions options2;
    options2.name = "Snapshot 2";
    
    std::string error;
    auto snap1 = service->create_snapshot(instanceId, gameDir, options1, nullptr, &error);
    ASSERT_TRUE(snap1.has_value()) << error;
    
    // Small delay to ensure unique timestamp
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    auto snap2 = service->create_snapshot(instanceId, gameDir, options2, nullptr, &error);
    ASSERT_TRUE(snap2.has_value()) << error;
    
    // List snapshots
    auto snapshots = service->list_snapshots_info(instanceId, &error);
    EXPECT_EQ(snapshots.size(), 2);
    
    // Find our snapshots
    bool found1 = false, found2 = false;
    for (const auto& snap : snapshots) {
        if (snap.name == "Snapshot 1") found1 = true;
        if (snap.name == "Snapshot 2") found2 = true;
    }
    EXPECT_TRUE(found1);
    EXPECT_TRUE(found2);
}

TEST_F(BackupServiceNewApiTest, RestoreSnapshotWithRollbackPoint) {
    // Create initial snapshot
    SnapshotCreateOptions options;
    options.name = "Original";
    options.includeSaves = true;
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
    ASSERT_TRUE(snapshot.has_value()) << error;
    
    // Modify game directory
    std::ofstream(gameDir / "saves" / "world1" / "new_file.txt") << "new content";
    
    // Restore with rollback point
    SnapshotRestoreOptions restoreOptions;
    restoreOptions.createRollbackPoint = true;
    restoreOptions.rollbackPointName = "Before Restore";
    
    auto callback = [](const std::string& stage, int progress, const std::string& detail) {
        // Progress callback
    };
    
    bool result = service->restore_snapshot(instanceId, snapshot->id, gameDir, restoreOptions, callback, &error);
    EXPECT_TRUE(result) << error;
    
    // Verify rollback point was created
    auto snapshots = service->list_snapshots_info(instanceId, &error);
    EXPECT_GE(snapshots.size(), 2);  // Original + rollback point
    
    bool foundRollback = false;
    for (const auto& snap : snapshots) {
        if (snap.name.find("Rollback") != std::string::npos) {
            foundRollback = true;
            break;
        }
    }
    EXPECT_TRUE(foundRollback);
}

TEST_F(BackupServiceNewApiTest, DeleteSnapshot) {
    // Create a snapshot
    SnapshotCreateOptions options;
    options.name = "To Delete";
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
    ASSERT_TRUE(snapshot.has_value()) << error;
    
    // Verify it exists
    EXPECT_TRUE(std::filesystem::exists(snapshot->archivePath));
    
    // Delete it
    bool result = service->delete_snapshot(instanceId, snapshot->id, &error);
    EXPECT_TRUE(result) << error;
    
    // Verify it's gone
    EXPECT_FALSE(std::filesystem::exists(snapshot->archivePath));
    auto retrieved = service->get_snapshot_info(instanceId, snapshot->id, &error);
    EXPECT_FALSE(retrieved.has_value());
}

TEST_F(BackupServiceNewApiTest, ExportAndImportSnapshot) {
    // Create a snapshot
    SnapshotCreateOptions options;
    options.name = "Export Test";
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
    ASSERT_TRUE(snapshot.has_value()) << error;
    
    // Export it
    auto exportPath = root / "exported" / "test-snapshot.zip";
    bool exportResult = service->export_snapshot(instanceId, snapshot->id, exportPath, nullptr, &error);
    EXPECT_TRUE(exportResult) << error;
    EXPECT_TRUE(std::filesystem::exists(exportPath));
    
    // Import it as a new snapshot
    auto imported = service->import_snapshot(instanceId, exportPath, &error);
    ASSERT_TRUE(imported.has_value()) << error;
    
    EXPECT_EQ(imported->instanceId, instanceId);
    EXPECT_EQ(imported->name, "Imported Snapshot");
    EXPECT_TRUE(std::filesystem::exists(imported->archivePath));
}

TEST_F(BackupServiceNewApiTest, VerifySnapshotIntegrity) {
    // Create a snapshot
    SnapshotCreateOptions options;
    options.name = "Integrity Test";
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
    ASSERT_TRUE(snapshot.has_value()) << error;
    
    // Verify integrity
    bool result = service->verify_snapshot_integrity(instanceId, snapshot->id, &error);
    EXPECT_TRUE(result) << error;
}

TEST_F(BackupServiceNewApiTest, ConfigureAutoBackup) {
    AutoBackupConfig config;
    config.enabled = true;
    config.backupBeforeLaunch = true;
    config.backupBeforeMajorUpdate = true;
    config.scheduleType = 1;  // Daily
    config.retainCount = 3;
    
    std::string error;
    bool result = service->configure_auto_backup(instanceId, config, &error);
    EXPECT_TRUE(result) << error;
    
    // Retrieve and verify config
    auto retrieved = service->get_auto_backup_config(instanceId, &error);
    EXPECT_TRUE(retrieved.enabled);
    EXPECT_TRUE(retrieved.backupBeforeLaunch);
    EXPECT_TRUE(retrieved.backupBeforeMajorUpdate);
    EXPECT_EQ(retrieved.scheduleType, 1);
    EXPECT_EQ(retrieved.retainCount, 3);
}

TEST_F(BackupServiceNewApiTest, AutoBackupBeforeLaunch) {
    // Configure auto backup
    AutoBackupConfig config;
    config.enabled = true;
    config.backupBeforeLaunch = true;
    
    std::string error;
    service->configure_auto_backup(instanceId, config, &error);
    
    // Trigger auto backup before launch
    auto snapshot = service->auto_backup_before_launch(instanceId, gameDir, nullptr, &error);
    ASSERT_TRUE(snapshot.has_value()) << error;
    
    EXPECT_EQ(snapshot->name, "Pre-launch Backup");
    EXPECT_TRUE(snapshot->isAutoBackup);
}

TEST_F(BackupServiceNewApiTest, AutoBackupBeforeMajorUpdate) {
    // Configure auto backup
    AutoBackupConfig config;
    config.enabled = true;
    config.backupBeforeMajorUpdate = true;
    
    std::string error;
    service->configure_auto_backup(instanceId, config, &error);
    
    // Trigger auto backup before major update
    auto snapshot = service->auto_backup_before_major_update(instanceId, gameDir, nullptr, &error);
    ASSERT_TRUE(snapshot.has_value()) << error;
    
    EXPECT_EQ(snapshot->name, "Pre-update Backup");
    EXPECT_TRUE(snapshot->isAutoBackup);
}

TEST_F(BackupServiceNewApiTest, CleanupOldSnapshots) {
    // Configure with retain count of 2
    AutoBackupConfig config;
    config.enabled = true;
    config.retainCount = 2;
    
    std::string error;
    service->configure_auto_backup(instanceId, config, &error);
    
    // Create 4 snapshots with small delays to ensure unique timestamps
    for (int i = 1; i <= 4; ++i) {
        SnapshotCreateOptions options;
        options.name = "Snapshot " + std::to_string(i);
        options.isAutoBackup = true;
        
        auto snapshot = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
        ASSERT_TRUE(snapshot.has_value()) << error;
        
        // Small delay to ensure unique timestamp
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    
    // Cleanup old snapshots
    int deleted = service->cleanup_old_snapshots(instanceId, &error);
    EXPECT_EQ(deleted, 2);  // Should delete 2 oldest
    
    // Verify only 2 remain
    auto snapshots = service->list_snapshots_info(instanceId, &error);
    EXPECT_EQ(snapshots.size(), 2);
}

TEST_F(BackupServiceNewApiTest, SnapshotSelectiveInclude) {
    // Create snapshot with only saves
    SnapshotCreateOptions options;
    options.name = "Saves Only";
    options.includeMods = false;
    options.includeResourcePacks = false;
    options.includeShaderPacks = false;
    options.includeSaves = true;
    options.includeConfig = false;
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, gameDir, options, nullptr, &error);
    ASSERT_TRUE(snapshot.has_value()) << error;
    
    // Verify only saves directory is included
    bool hasSaves = false;
    bool hasMods = false;
    
    for (const auto& path : snapshot->includedPaths) {
        if (path.find("saves") != std::string::npos) hasSaves = true;
        if (path.find("mods") != std::string::npos) hasMods = true;
    }
    
    EXPECT_TRUE(hasSaves);
    EXPECT_FALSE(hasMods);
}

TEST_F(BackupServiceNewApiTest, SnapshotWithInvalidGameDir) {
    SnapshotCreateOptions options;
    options.name = "Invalid Test";
    
    std::string error;
    auto snapshot = service->create_snapshot(instanceId, "/nonexistent/path", options, nullptr, &error);
    
    EXPECT_FALSE(snapshot.has_value());
    EXPECT_FALSE(error.empty());
}

TEST_F(BackupServiceNewApiTest, RestoreNonExistentSnapshot) {
    SnapshotRestoreOptions options;
    
    std::string error;
    bool result = service->restore_snapshot(instanceId, "nonexistent-id", gameDir, options, nullptr, &error);
    
    EXPECT_FALSE(result);
    EXPECT_FALSE(error.empty());
}
