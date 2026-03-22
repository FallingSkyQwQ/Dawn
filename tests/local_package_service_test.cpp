#include "dawn/core/local/local_package_service.h"
#include "dawn/infra/fs/file_system.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

using namespace dawn::core;

namespace {

void append_u16(std::string* bytes, std::uint16_t value) {
    bytes->push_back(static_cast<char>(value & 0xFF));
    bytes->push_back(static_cast<char>((value >> 8) & 0xFF));
}

void append_u32(std::string* bytes, std::uint32_t value) {
    append_u16(bytes, static_cast<std::uint16_t>(value & 0xFFFF));
    append_u16(bytes, static_cast<std::uint16_t>((value >> 16) & 0xFFFF));
}

void append_central_directory_entry(std::string* bytes, const std::string& name) {
    append_u32(bytes, 0x02014b50);
    append_u16(bytes, 20);
    append_u16(bytes, 20);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    append_u16(bytes, static_cast<std::uint16_t>(name.size()));
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u16(bytes, 0);
    append_u32(bytes, 0);
    append_u32(bytes, 0);
    bytes->append(name);
}

std::string make_fake_zip(const std::vector<std::string>& entries) {
    std::string bytes;
    for (const auto& entry : entries) {
        append_central_directory_entry(&bytes, entry);
    }

    std::string eocd;
    append_u32(&eocd, 0x06054b50);
    append_u16(&eocd, 0);
    append_u16(&eocd, 0);
    append_u16(&eocd, static_cast<std::uint16_t>(entries.size()));
    append_u16(&eocd, static_cast<std::uint16_t>(entries.size()));
    append_u32(&eocd, static_cast<std::uint32_t>(bytes.size()));
    append_u32(&eocd, 0);
    append_u16(&eocd, 0);
    bytes += eocd;
    return bytes;
}

} // namespace

TEST(LocalPackageService, DetectsCommonLocalPackageTypes) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-local-package-service";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto modPath = root / "example-mod.jar";
    const auto resourcepackPath = root / "example-resource.zip";
    const auto shaderPath = root / "example-shader.zip";
    const auto modpackPath = root / "example-pack.mrpack";
    const auto unknownPath = root / "example-blob.zip";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modPath, make_fake_zip({"fabric.mod.json", "assets/example/lang/en_us.json"}), &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(resourcepackPath, make_fake_zip({"pack.mcmeta", "assets/minecraft/textures/block.png"}), &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(shaderPath, make_fake_zip({"shaders.properties", "shaders/program/example.fsh"}), &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modpackPath, make_fake_zip({"modrinth.index.json", "overrides/config/example.txt"}), &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(unknownPath, make_fake_zip({"docs/readme.txt", "assets/random.bin"}), &error)) << error;

    LocalPackageService service;

    const auto mod = service.analyze(modPath);
    EXPECT_EQ(mod.type, LocalPackageType::Mod);
    EXPECT_TRUE(mod.archive);
    EXPECT_FALSE(mod.archiveEntries.empty());
    EXPECT_GT(mod.confidence, 0.0);

    const auto resourcepack = service.analyze(resourcepackPath);
    EXPECT_EQ(resourcepack.type, LocalPackageType::Resourcepack);
    EXPECT_TRUE(resourcepack.archive);
    EXPECT_FALSE(resourcepack.archiveEntries.empty());
    EXPECT_GT(resourcepack.confidence, 0.0);

    const auto shader = service.analyze(shaderPath);
    EXPECT_EQ(shader.type, LocalPackageType::Shader);
    EXPECT_TRUE(shader.archive);
    EXPECT_FALSE(shader.archiveEntries.empty());
    EXPECT_GT(shader.confidence, 0.0);

    const auto modpack = service.analyze(modpackPath);
    EXPECT_EQ(modpack.type, LocalPackageType::Modpack);
    EXPECT_TRUE(modpack.archive);
    EXPECT_FALSE(modpack.archiveEntries.empty());
    EXPECT_GT(modpack.confidence, 0.0);

    const auto unknown = service.analyze(unknownPath);
    EXPECT_EQ(unknown.type, LocalPackageType::Unknown);
    EXPECT_FALSE(unknown.archiveEntries.empty());
    EXPECT_TRUE(unknown.reasons.empty() || !unknown.reasons.front().empty());

    std::filesystem::remove_all(root);
}

