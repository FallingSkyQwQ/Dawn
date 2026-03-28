#pragma once

#include "dawn/core/minecraft/version_package.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dawn::core {

struct MinecraftVersionInfo {
    std::string versionId;
    std::string channel;
    std::string releaseDate;
    int recommendedJavaMajor = 17;
    std::string notes;
};

// Version download progress callback
using VersionDownloadCallback = std::function<void(
    const std::string& stage,      // "metadata", "client", "libraries", "assets"
    std::size_t current,
    std::size_t total,
    const std::string& message)>;

// Version download result
struct VersionDownloadResult {
    bool success = false;
    std::string versionId;
    std::filesystem::path versionJsonPath;
    std::filesystem::path clientJarPath;
    std::filesystem::path librariesDir;
    std::filesystem::path nativesDir;
    std::size_t librariesDownloaded = 0;
    std::size_t librariesSkipped = 0;
    std::size_t librariesFailed = 0;
    std::vector<std::string> errors;
    MinecraftVersionPackage package;
};

class MinecraftService {
public:
    std::vector<MinecraftVersionInfo> list_versions() const;
    MinecraftVersionInfo find_version(const std::string& versionId) const;
    std::string classify(const std::string& versionId) const;

    // Download a specific Minecraft version
    // Returns true on success, false on failure with details in result
    [[nodiscard]] bool download_version(
        const std::string& versionId,
        const std::filesystem::path& outputDir,
        VersionDownloadResult* result,
        VersionDownloadCallback callback = nullptr,
        int maxConcurrency = 4) const;

    // Fetch version package JSON from Mojang
    [[nodiscard]] bool fetch_version_package(
        const std::string& versionId,
        MinecraftVersionPackage* package,
        std::string* error = nullptr) const;

    // Download client jar for version
    [[nodiscard]] bool download_client_jar(
        const MinecraftVersionPackage& package,
        const std::filesystem::path& outputPath,
        std::string* error = nullptr,
        VersionDownloadCallback callback = nullptr) const;

    // Get version manifest URL
    [[nodiscard]] static std::string version_manifest_url();

    // Get default libraries directory
    [[nodiscard]] static std::filesystem::path default_libraries_directory();

    // Get default versions directory
    [[nodiscard]] static std::filesystem::path default_versions_directory();
};

} // namespace dawn::core
