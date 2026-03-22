#include "dawn/core/backup/backup_service.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

using namespace dawn::core;

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
