#include "dawn/core/settings/settings_service.h"
#include "dawn/infra/fs/file_system.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace dawn::core;

TEST(GlobalSettingsService, RoundTripFile) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-settings-test";
    std::filesystem::remove_all(root);

    SettingsService service(root);
    GlobalSettings settings;
    settings.theme = "light";
    settings.downloadConcurrency = 8;
    settings.cachePath = root / "cache";
    settings.javaStrategy = JavaStrategy::CustomPath;
    settings.javaRuntimePath = "C:/Java/bin/java.exe";
    settings.memoryStrategy = MemoryStrategy::Performance;
    settings.minMemoryMb = 4096;
    settings.maxMemoryMb = 8192;
    settings.backupStrategy = BackupStrategy::BeforeLaunch;
    settings.firstLaunchCompleted = true;
    settings.uiMode = UiMode::Advanced;
    settings.lowDiskThresholdGb = 64;
    settings.proxy.enabled = true;
    settings.proxy.host = "127.0.0.1";
    settings.proxy.port = 8080;
    settings.experimentalMode = true;
    settings.experimentalFlags = {"feature-a", "feature-b"};

    std::string error;
    ASSERT_TRUE(service.save(settings, &error)) << error;

    const auto loaded = service.load(&error);
    EXPECT_EQ(loaded.theme, "light");
    EXPECT_EQ(loaded.downloadConcurrency, 8);
    EXPECT_EQ(loaded.javaStrategy, JavaStrategy::CustomPath);
    EXPECT_EQ(loaded.memoryStrategy, MemoryStrategy::Performance);
    EXPECT_EQ(loaded.backupStrategy, BackupStrategy::BeforeLaunch);
    EXPECT_TRUE(loaded.firstLaunchCompleted);
    EXPECT_EQ(loaded.uiMode, UiMode::Advanced);
    EXPECT_EQ(loaded.lowDiskThresholdGb, 64);
    EXPECT_TRUE(loaded.proxy.enabled);
    EXPECT_EQ(loaded.experimentalFlags.size(), 2u);

    std::filesystem::remove_all(root);
}

TEST(GlobalSettingsService, DetectsLowDiskSpaceWithLargeThreshold) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-settings-disk-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    std::string error;
    const auto result = SettingsService::check_low_disk_space(root, 1000000, &error);
    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(result.low);
    EXPECT_GT(result.availableBytes, 0u);
    EXPECT_GT(result.thresholdBytes, 0u);
    EXPECT_EQ(result.path, root);
    EXPECT_FALSE(result.message.empty());

    std::filesystem::remove_all(root);
}

TEST(GlobalSettingsService, CleansCacheAndReportsSummary) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-settings-cache-test";
    const auto cache = root / "cache";
    std::filesystem::remove_all(root);

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(cache / "alpha.bin", "abc", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(cache / "nested" / "beta.bin", "12345", &error)) << error;

    SettingsService service(root);
    const auto result = service.clean_cache(cache, &error);

    EXPECT_TRUE(error.empty());
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.cachePath, cache);
    EXPECT_EQ(result.bytesBefore, 8u);
    EXPECT_EQ(result.bytesAfter, 0u);
    EXPECT_EQ(result.bytesFreed, 8u);
    EXPECT_EQ(result.filesRemoved, 2u);
    EXPECT_FALSE(std::filesystem::exists(cache / "alpha.bin"));
    EXPECT_FALSE(std::filesystem::exists(cache / "nested" / "beta.bin"));
    EXPECT_TRUE(std::filesystem::exists(cache));
    EXPECT_FALSE(result.logs.empty());

    std::filesystem::remove_all(root);
}
