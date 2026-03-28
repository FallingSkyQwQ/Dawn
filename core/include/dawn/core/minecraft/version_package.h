#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace dawn::core {

// Library rule condition for OS/architecture filtering
struct LibraryRule {
    std::string action;  // "allow" or "disallow"
    std::optional<std::string> osName;      // "windows", "linux", "osx"
    std::optional<std::string> osVersion;   // Windows version regex
    std::optional<std::string> osArch;      // "x86", "x64"
};

// Library download information
struct LibraryDownload {
    std::string path;       // Maven path (e.g., "com/mojang/brigadier/1.0.17/brigadier-1.0.17.jar")
    std::string url;        // Download URL
    std::string sha1;       // SHA1 hash
    std::size_t size = 0;   // File size in bytes
};

// Library artifact (main or classifier)
struct LibraryArtifact {
    std::string path;
    std::string url;
    std::string sha1;
    std::size_t size = 0;
};

// Native library downloads (classifiers)
struct NativeDownloads {
    std::optional<LibraryArtifact> linux;
    std::optional<LibraryArtifact> windows;
    std::optional<LibraryArtifact> osx;
};

// Library entry in version package
struct Library {
    std::string name;                       // Maven coordinate (e.g., "com.mojang:brigadier:1.0.17")
    std::optional<std::string> url;         // Custom repository URL (if not Maven Central)
    std::optional<LibraryDownload> download; // Pre-computed download info
    std::optional<NativeDownloads> natives; // Native library downloads
    std::vector<std::string> extractExclude; // Patterns to exclude from native extraction
    std::vector<LibraryRule> rules;         // Filtering rules
    bool isNative = false;                  // Whether this is a native library

    // Parsed Maven coordinates
    [[nodiscard]] std::string group_id() const;
    [[nodiscard]] std::string artifact_id() const;
    [[nodiscard]] std::string version() const;
    [[nodiscard]] std::string classifier() const;
    [[nodiscard]] std::string extension() const;
};

// Asset index information
struct AssetIndex {
    std::string id;
    std::string url;
    std::string sha1;
    std::size_t size = 0;
    std::size_t totalSize = 0;  // Uncompressed size
};

// Client/Server download info
struct DownloadArtifact {
    std::string url;
    std::string sha1;
    std::size_t size = 0;
};

struct Downloads {
    std::optional<DownloadArtifact> client;
    std::optional<DownloadArtifact> server;
    std::optional<DownloadArtifact> client_mappings;
    std::optional<DownloadArtifact> server_mappings;
    std::optional<DownloadArtifact> windows_server;
};

// Java version requirement
struct JavaVersion {
    std::string component;  // "java-runtime-alpha", "java-runtime-beta", etc.
    int majorVersion = 17;  // Java major version
};

// Logging configuration
struct LoggingConfig {
    std::string argument;   // JVM argument with placeholder
    struct LoggingFile {
        std::string id;
        std::string url;
        std::string sha1;
        std::size_t size = 0;
    };
    std::optional<LoggingFile> file;
};

// Complete Minecraft version package
struct MinecraftVersionPackage {
    std::string versionId;
    std::string type;           // "release", "snapshot", etc.
    std::string releaseTime;
    std::string time;
    std::string inheritsFrom;   // For inherited versions (Forge, etc.)

    // Downloads
    Downloads downloads;

    // Libraries
    std::vector<Library> libraries;

    // Asset index
    AssetIndex assetIndex;

    // Launch configuration
    std::string mainClass;
    std::optional<std::string> minecraftArguments;  // Pre-1.13
    std::vector<std::string> jvmArguments;          // 1.13+
    std::vector<std::string> gameArguments;         // 1.13+

    // Java requirements
    std::optional<JavaVersion> javaVersion;

    // Logging
    std::optional<LoggingConfig> logging;

    // Compliance
    std::optional<std::string> complianceLevel;

    // Minimum launcher version
    int minimumLauncherVersion = 0;

    // Asset index ID (legacy)
    std::string assets;

    // Inheritance chain (if applicable)
    std::vector<MinecraftVersionPackage> inheritanceChain;
};

// Parse version package from JSON string
// Returns true on success, false on failure with error message in *error
[[nodiscard]] bool parse_version_package(
    const std::string& json,
    MinecraftVersionPackage* package,
    std::string* error = nullptr);

// Parse version package from JSON file
[[nodiscard]] bool parse_version_package_file(
    const std::filesystem::path& path,
    MinecraftVersionPackage* package,
    std::string* error = nullptr);

// Check if a library should be used on the current platform
[[nodiscard]] bool should_use_library(const Library& library);

// Get the native classifier for the current platform
[[nodiscard]] std::optional<std::string> get_native_classifier();

// Get platform name for rule matching
[[nodiscard]] std::string get_platform_name();

// Get architecture name for rule matching
[[nodiscard]] std::string get_architecture_name();

} // namespace dawn::core
