#include "dawn/core/service/preflight_service.h"

#include <gtest/gtest.h>

using namespace dawn::core;

TEST(PreflightService, ReportsBlockingErrors) {
    InstanceManifest manifest;
    manifest.name = "";
    manifest.mcVersion = "";
    manifest.gameDir = "";

    const auto result = PreflightService{}.inspect(manifest);
    EXPECT_FALSE(result.ready);
    ASSERT_FALSE(result.issues.empty());
    EXPECT_EQ(result.issues.front().severity, PreflightSeverity::Error);
}

TEST(PreflightService, AcceptsValidManifest) {
    InstanceManifest manifest;
    manifest.name = "Dawn Sandbox";
    manifest.mcVersion = "1.20.1";
    manifest.gameDir = "D:/Dawn/instances/instance-001/game";
    manifest.javaProfileId = "java-17";

    const auto result = PreflightService{}.inspect(manifest);
    EXPECT_TRUE(result.ready);
}
