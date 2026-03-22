#include "dawn/core/local/local_package_service.h"
#include "dawn/infra/fs/file_system.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace dawn::core;

TEST(LocalPackageService, DetectsCommonLocalPackageTypes) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-local-package-service";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto modPath = root / "example-mod.jar";
    const auto resourcepackPath = root / "example-resource.zip";
    const auto shaderPath = root / "example-shader.zip";
    const auto modpackPath = root / "example-pack.mrpack";
    const auto unknownPath = root / "example-blob.bin";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modPath, "fabric.mod.json", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(resourcepackPath, "pack.mcmeta", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(shaderPath, "shaders.properties", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modpackPath, "modrinth.index.json", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(unknownPath, "plain text with no signature", &error)) << error;

    LocalPackageService service;

    const auto mod = service.analyze(modPath);
    EXPECT_EQ(mod.type, LocalPackageType::Mod);
    EXPECT_TRUE(mod.archive);
    EXPECT_GT(mod.confidence, 0.0);

    const auto resourcepack = service.analyze(resourcepackPath);
    EXPECT_EQ(resourcepack.type, LocalPackageType::Resourcepack);
    EXPECT_TRUE(resourcepack.archive);
    EXPECT_GT(resourcepack.confidence, 0.0);

    const auto shader = service.analyze(shaderPath);
    EXPECT_EQ(shader.type, LocalPackageType::Shader);
    EXPECT_TRUE(shader.archive);
    EXPECT_GT(shader.confidence, 0.0);

    const auto modpack = service.analyze(modpackPath);
    EXPECT_EQ(modpack.type, LocalPackageType::Modpack);
    EXPECT_TRUE(modpack.archive);
    EXPECT_GT(modpack.confidence, 0.0);

    const auto unknown = service.analyze(unknownPath);
    EXPECT_EQ(unknown.type, LocalPackageType::Unknown);
    EXPECT_TRUE(unknown.reasons.empty() || !unknown.reasons.front().empty());

    std::filesystem::remove_all(root);
}

