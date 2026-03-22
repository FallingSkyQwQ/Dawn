#include "dawn/core/java/java_service.h"
#include "dawn/core/loaders/loader_service.h"
#include "dawn/core/minecraft/minecraft_service.h"

#include <gtest/gtest.h>

#include <algorithm>

using namespace dawn::core;

TEST(RuntimeServices, MinecraftVersionsReturnDataWithoutStubNotes) {
    MinecraftService service;
    const auto versions = service.list_versions();
    ASSERT_FALSE(versions.empty());
    EXPECT_TRUE(std::all_of(versions.begin(), versions.end(), [](const MinecraftVersionInfo& info) {
        return !info.versionId.empty() && info.notes.find("stub") == std::string::npos;
    }));
}

TEST(RuntimeServices, LoaderProfilesCoverAllCoreLoaders) {
    LoaderService service;
    const auto loaders = service.list_loaders("1.20.1");
    ASSERT_FALSE(loaders.empty());

    auto has_type = [&](LoaderType type) {
        return std::any_of(loaders.begin(), loaders.end(), [&](const LoaderProfile& profile) {
            return profile.loaderType == type && !profile.versionId.empty();
        });
    };

    EXPECT_TRUE(has_type(LoaderType::Fabric));
    EXPECT_TRUE(has_type(LoaderType::Quilt));
    EXPECT_TRUE(has_type(LoaderType::Forge));
    EXPECT_TRUE(has_type(LoaderType::NeoForge));
    EXPECT_TRUE(has_type(LoaderType::OptiFine));
}

TEST(RuntimeServices, RecommendedJavaRuntimeIsNotStubNamed) {
    JavaService service;
    const auto runtime = service.recommended_runtime();
    EXPECT_TRUE(runtime.id.find("stub") == std::string::npos);
    EXPECT_TRUE(runtime.versionText.find("stub") == std::string::npos);
}
