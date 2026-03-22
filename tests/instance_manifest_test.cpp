#include "dawn/core/serialization/manifest_codec.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace dawn::core;

TEST(InstanceManifestSerialization, RoundTripFile) {
    InstanceManifest manifest;
    manifest.id = "instance-001";
    manifest.name = "Dawn Sandbox";
    manifest.icon = "icon.png";
    manifest.mcVersion = "1.20.1";
    manifest.loaderType = LoaderType::Fabric;
    manifest.loaderVersion = "0.15.11";
    manifest.optifineVersion = "";
    manifest.javaProfileId = "java-17";
    manifest.memoryProfile = "4G";
    manifest.gameDir = "D:/Dawn/instances/instance-001/game";
    manifest.createdAt = "2026-03-22T00:00:00";
    manifest.lastPlayedAt = "";
    manifest.tags = {"alpha", "sandbox"};
    manifest.notes = "Round-trip test";
    manifest.themeColor = "#66a3ff";

    const auto temp_path = std::filesystem::temp_directory_path() / "dawn-instance-test.json";
    std::string error;
    ASSERT_TRUE(save_instance_manifest(temp_path, manifest, &error)) << error;

    InstanceManifest loaded;
    ASSERT_TRUE(load_instance_manifest(temp_path, &loaded, &error)) << error;

    EXPECT_EQ(loaded.id, manifest.id);
    EXPECT_EQ(loaded.name, manifest.name);
    EXPECT_EQ(loaded.loaderType, manifest.loaderType);
    EXPECT_EQ(loaded.loaderVersion, manifest.loaderVersion);
    EXPECT_EQ(loaded.tags.size(), 2u);
    EXPECT_EQ(loaded.tags.front(), "alpha");
    EXPECT_EQ(loaded.themeColor, manifest.themeColor);

    std::error_code ec;
    std::filesystem::remove(temp_path, ec);
}
