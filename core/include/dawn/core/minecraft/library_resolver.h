#pragma once

#include "dawn/core/minecraft/version_package.h"
#include "dawn/core/model/task_types.h"

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace dawn::core {

// Resolved library with computed download information
struct ResolvedLibrary {
    Library source;

    // Computed download URL
    std::string downloadUrl;

    // Local storage path (relative to libraries directory)
    std::filesystem::path localPath;

    // SHA1 checksum
    std::string sha1;

    // File size
    std::size_t size = 0;

    // Whether this is a native library
    bool isNative = false;

    // For native libraries: extraction path
    std::optional<std::filesystem::path> extractPath;

    // Patterns to exclude from extraction
    std::vector<std::string> extractExclude;
};

// Library resolution result
struct LibraryResolutionResult {
    std::vector<ResolvedLibrary> libraries;
    std::vector<ResolvedLibrary> nativeLibraries;
    std::size_t totalSize = 0;
    std::size_t nativeSize = 0;
    std::vector<std::string> errors;
};

// Download progress callback
using LibraryDownloadCallback = std::function<void(
    const std::string& libraryName,
    std::size_t downloaded,
    std::size_t total)>;

// Library download result
struct LibraryDownloadResult {
    bool success = false;
    std::string libraryName;
    std::filesystem::path destination;
    std::string error;
    bool verified = false;
    bool extracted = false;  // For native libraries
};

// Batch download result
struct LibraryBatchDownloadResult {
    std::vector<LibraryDownloadResult> results;
    std::size_t succeeded = 0;
    std::size_t failed = 0;
    std::size_t skipped = 0;  // Already exists and valid
    std::vector<std::string> errors;
};

// Native extraction result
struct NativeExtractionResult {
    bool success = false;
    std::string libraryName;
    std::filesystem::path sourcePath;
    std::filesystem::path extractDir;
    std::vector<std::filesystem::path> extractedFiles;
    std::string error;
};

class LibraryResolver {
public:
    LibraryResolver();
    explicit LibraryResolver(std::string defaultRepository);

    // Resolve libraries from version package
    // Filters libraries based on platform rules and computes download URLs
    [[nodiscard]] LibraryResolutionResult resolve_libraries(
        const MinecraftVersionPackage& package) const;

    // Download a single library
    // Returns true on success, false on failure with error in *error
    [[nodiscard]] bool download_library(
        const ResolvedLibrary& library,
        const std::filesystem::path& outputDir,
        std::string* error = nullptr,
        LibraryDownloadCallback callback = nullptr) const;

    // Download multiple libraries
    [[nodiscard]] LibraryBatchDownloadResult download_libraries(
        const std::vector<ResolvedLibrary>& libraries,
        const std::filesystem::path& outputDir,
        int maxConcurrency = 4,
        LibraryDownloadCallback callback = nullptr) const;

    // Check if library exists and has valid SHA1
    [[nodiscard]] bool check_library_integrity(
        const ResolvedLibrary& library,
        const std::filesystem::path& librariesDir) const;

    // Check if library file exists and has valid SHA1
    [[nodiscard]] static bool check_library_integrity(
        const std::filesystem::path& filePath,
        const std::string& expectedSha1);

    // Extract native libraries to natives directory
    [[nodiscard]] NativeExtractionResult extract_natives(
        const ResolvedLibrary& nativeLibrary,
        const std::filesystem::path& sourcePath,
        const std::filesystem::path& nativesDir) const;

    // Extract all native libraries
    [[nodiscard]] std::vector<NativeExtractionResult> extract_all_natives(
        const std::vector<ResolvedLibrary>& nativeLibraries,
        const std::filesystem::path& librariesDir,
        const std::filesystem::path& nativesDir) const;

    // Build classpath from resolved libraries
    [[nodiscard]] std::string build_classpath(
        const std::vector<ResolvedLibrary>& libraries,
        const std::filesystem::path& librariesDir,
        const std::filesystem::path& clientJar) const;

    // Build native library path for JVM
    [[nodiscard]] std::string build_native_path(
        const std::vector<ResolvedLibrary>& nativeLibraries,
        const std::filesystem::path& nativesDir) const;

    // Set custom repository URL
    void set_default_repository(const std::string& url);
    [[nodiscard]] std::string default_repository() const;

    // Add fallback repository
    void add_fallback_repository(const std::string& url);
    [[nodiscard]] const std::vector<std::string>& fallback_repositories() const;

    // Enable/disable native library extraction
    void set_extract_natives(bool extract);
    [[nodiscard]] bool extract_natives() const;

private:
    [[nodiscard]] std::string build_maven_url(
        const Library& library,
        const std::string& repository) const;
    [[nodiscard]] std::string build_maven_path(const Library& library) const;
    [[nodiscard]] std::optional<ResolvedLibrary> resolve_single_library(
        const Library& library) const;
    [[nodiscard]] std::optional<std::string> select_native_artifact_url(
        const Library& library) const;
    [[nodiscard]] std::optional<std::string> select_native_local_path(
        const Library& library) const;
    [[nodiscard]] bool should_extract_file(
        const std::string& filename,
        const std::vector<std::string>& excludePatterns) const;

    std::string defaultRepository_;
    std::vector<std::string> fallbackRepositories_;
    bool extractNatives_ = true;
};

// Utility functions

// Parse Maven coordinate string into components
// Format: groupId:artifactId:version[:classifier][@extension]
[[nodiscard]] bool parse_maven_coordinate(
    const std::string& coordinate,
    std::string* groupId,
    std::string* artifactId,
    std::string* version,
    std::string* classifier = nullptr,
    std::string* extension = nullptr);

// Build Maven path from coordinate components
[[nodiscard]] std::string build_maven_path(
    const std::string& groupId,
    const std::string& artifactId,
    const std::string& version,
    const std::string& classifier = {},
    const std::string& extension = "jar");

// Build Maven URL
[[nodiscard]] std::string build_maven_url(
    const std::string& repository,
    const std::string& groupId,
    const std::string& artifactId,
    const std::string& version,
    const std::string& classifier = {},
    const std::string& extension = "jar");

// Get the classifier for natives on current platform
[[nodiscard]] std::string get_native_classifier_string();

} // namespace dawn::core
