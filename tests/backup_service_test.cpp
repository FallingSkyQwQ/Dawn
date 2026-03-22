#include "dawn/core/backup/backup_service.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace dawn::core;

TEST(BackupService, CreatesSnapshotManifestAndRestorePlan) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-backup-test";
    std::filesystem::remove_all(root);

    BackupService service(root);
    InstanceManifest manifest;
    manifest.id = "instance-01";
    manifest.name = "Test Instance";

    std::string error;
    const auto snapshot = service.create_snapshot_manifest(manifest, "before update", &error);
    ASSERT_FALSE(snapshot.snapshotId.empty()) << error;
    EXPECT_EQ(snapshot.instanceId, "instance-01");

    const auto snapshots = service.list_snapshots("instance-01", &error);
    ASSERT_FALSE(snapshots.empty()) << error;
    EXPECT_EQ(snapshots.front().instanceId, "instance-01");

    const auto plan = service.build_restore_plan(snapshot);
    EXPECT_EQ(plan.snapshotId, snapshot.snapshotId);
    EXPECT_EQ(plan.taskPlan.steps.size(), 3u);
    EXPECT_TRUE(plan.reversible);

    std::filesystem::remove_all(root);
}
