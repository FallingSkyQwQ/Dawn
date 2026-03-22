#include "dawn/core/settings/settings_service.h"

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
    EXPECT_TRUE(loaded.proxy.enabled);
    EXPECT_EQ(loaded.experimentalFlags.size(), 2u);

    std::filesystem::remove_all(root);
}
