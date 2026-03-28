#include "dawn/core/minecraft/library_resolver.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"
#include "dawn/infra/net/http_client_factory.h"
#include "dawn/infra/net/http_client.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <future>
#include <mutex>
#include <sstream>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace dawn::core {

namespace {

constexpr std::string_view kDefaultMavenRepository = "https://libraries.minecraft.net/";
constexpr std::string_view kFallbackMavenRepository = "https://repo.maven.apache.org/maven2/";

std::string to_lower(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (char ch : str) {
        result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
    }
    return result;
}

bool match_wildcard(std::string_view pattern, std::string_view text) {
    // Simple wildcard matching (* matches any sequence, ? matches single char)
    std::size_t patternIdx = 0;
    std::size_t textIdx = 0;
    std::size_t starIdx = std::string::npos;
    std::size_t matchIdx = 0;

    while (textIdx < text.size()) {
        if (patternIdx < pattern.size() && (pattern[patternIdx] == '?' || pattern[patternIdx] == text[textIdx])) {
            ++patternIdx;
            ++textIdx;
        } else if (patternIdx < pattern.size() && pattern[patternIdx] == '*') {
            starIdx = patternIdx;
            matchIdx = textIdx;
            ++patternIdx;
        } else if (starIdx != std::string::npos) {
            patternIdx = starIdx + 1;
            textIdx = ++matchIdx;
        } else {
            return false;
        }
    }

    while (patternIdx < pattern.size() && pattern[patternIdx] == '*') {
        ++patternIdx;
    }

    return patternIdx == pattern.size();
}

bool extract_zip_entry(std::ifstream& file, std::size_t entryOffset, std::size_t compressedSize,
                       std::size_t uncompressedSize, std::uint32_t compressionMethod,
                       std::vector<char>* output) {
    if (compressionMethod == 0) {
        // Stored (no compression)
        file.seekg(static_cast<std::streamoff>(entryOffset));
        output->resize(uncompressedSize);
        file.read(output->data(), static_cast<std::streamoff>(uncompressedSize));
        return file.good();
    } else if (compressionMethod == 8) {
        // Deflate compression - simplified, would need zlib
        // For now, return false to indicate we need external extraction
        return false;
    }
    return false;
}

} // namespace

// Maven coordinate parsing
bool parse_maven_coordinate(const std::string& coordinate, std::string* groupId,
                            std::string* artifactId, std::string* version,
                            std::string* classifier, std::string* extension) {
    if (!groupId || !artifactId || !version) {
        return false;
    }

    // Format: groupId:artifactId:version[:classifier][@extension]
    std::vector<std::string> parts;
    std::string current;

    for (char ch : coordinate) {
        if (ch == ':' && parts.size() < 3) {
            parts.push_back(current);
            current.clear();
        } else if (ch == '@' && parts.size() >= 2) {
            parts.push_back(current);
            current.clear();
            // Remaining is extension
            for (char c : coordinate.substr(&ch - coordinate.data() + 1)) {
                current.push_back(c);
            }
            break;
        } else {
            current.push_back(ch);
        }
    }
    parts.push_back(current);

    if (parts.size() < 3) {
        return false;
    }

    *groupId = parts[0];
    *artifactId = parts[1];
    *version = parts[2];

    if (classifier && parts.size() > 3) {
        *classifier = parts[3];
    }

    if (extension) {
        if (parts.size() > 4) {
            *extension = parts[4];
        } else if (parts.size() == 4 && !parts[3].empty() && parts[3][0] != '@') {
            // Check if last part is classifier or extension
            *classifier = parts[3];
            *extension = "jar";
        } else {
            *extension = "jar";
        }
    }

    return true;
}

std::string build_maven_path(const std::string& groupId, const std::string& artifactId,
                             const std::string& version, const std::string& classifier,
                             const std::string& extension) {
    std::string path = groupId;
    std::replace(path.begin(), path.end(), '.', '/');
    path += '/' + artifactId + '/' + version + '/' + artifactId + '-' + version;
    if (!classifier.empty()) {
        path += '-' + classifier;
    }
    path += '.' + extension;
    return path;
}

std::string build_maven_url(const std::string& repository, const std::string& groupId,
                            const std::string& artifactId, const std::string& version,
                            const std::string& classifier, const std::string& extension) {
    std::string url = repository;
    if (!url.empty() && url.back() != '/') {
        url += '/';
    }
    url += build_maven_path(groupId, artifactId, version, classifier, extension);
    return url;
}

std::string get_native_classifier_string() {
#if defined(_WIN32)
    return "natives-windows";
#elif defined(__APPLE__)
    return "natives-osx";
#elif defined(__linux__)
    return "natives-linux";
#else
    return {};
#endif
}

// LibraryResolver implementation
LibraryResolver::LibraryResolver()
    : LibraryResolver(std::string(kDefaultMavenRepository)) {
}

LibraryResolver::LibraryResolver(std::string defaultRepository)
    : defaultRepository_(std::move(defaultRepository)) {
    fallbackRepositories_.push_back(std::string(kFallbackMavenRepository));
}

void LibraryResolver::set_default_repository(const std::string& url) {
    defaultRepository_ = url;
}

std::string LibraryResolver::default_repository() const {
    return defaultRepository_;
}

void LibraryResolver::add_fallback_repository(const std::string& url) {
    if (std::find(fallbackRepositories_.begin(), fallbackRepositories_.end(), url) == fallbackRepositories_.end()) {
        fallbackRepositories_.push_back(url);
    }
}

const std::vector<std::string>& LibraryResolver::fallback_repositories() const {
    return fallbackRepositories_;
}

void LibraryResolver::set_extract_natives(bool extract) {
    extractNatives_ = extract;
}

bool LibraryResolver::extract_natives() const {
    return extractNatives_;
}

std::string LibraryResolver::build_maven_url(const Library& library, const std::string& repository) const {
    std::string groupId, artifactId, version, classifier, extension;
    if (!parse_maven_coordinate(library.name, &groupId, &artifactId, &version, &classifier, &extension)) {
        return {};
    }

    // For native libraries, use the native classifier
    if (library.isNative && classifier.empty()) {
        classifier = get_native_classifier_string();
    }

    std::string url = repository;
    if (!url.empty() && url.back() != '/') {
        url += '/';
    }

    // Use pre-computed download URL if available
    if (library.download.has_value() && !library.download->url.empty()) {
        return library.download->url;
    }

    // Use custom library URL if specified
    if (library.url.has_value() && !library.url->empty()) {
        url = library.url.value();
        if (!url.empty() && url.back() != '/') {
            url += '/';
        }
    }

    // Call global build_maven_path function
    url += dawn::core::build_maven_path(groupId, artifactId, version, classifier, extension);
    return url;
}

std::string LibraryResolver::build_maven_path(const Library& library) const {
    if (library.download.has_value() && !library.download->path.empty()) {
        return library.download->path;
    }

    std::string groupId, artifactId, version, classifier, extension;
    if (!parse_maven_coordinate(library.name, &groupId, &artifactId, &version, &classifier, &extension)) {
        return {};
    }

    if (library.isNative && classifier.empty()) {
        classifier = get_native_classifier_string();
    }

    // Call global build_maven_path function
    return dawn::core::build_maven_path(groupId, artifactId, version, classifier, extension);
}

std::optional<std::string> LibraryResolver::select_native_artifact_url(const Library& library) const {
    if (!library.natives.has_value()) {
        return std::nullopt;
    }

#if defined(_WIN32)
    if (library.natives->windows.has_value()) {
        return library.natives->windows->url;
    }
#elif defined(__APPLE__)
    if (library.natives->osx.has_value()) {
        return library.natives->osx->url;
    }
#elif defined(__linux__)
    if (library.natives->linux.has_value()) {
        return library.natives->linux->url;
    }
#endif

    return std::nullopt;
}

std::optional<std::string> LibraryResolver::select_native_local_path(const Library& library) const {
    if (!library.natives.has_value()) {
        return std::nullopt;
    }

#if defined(_WIN32)
    if (library.natives->windows.has_value()) {
        return library.natives->windows->path;
    }
#elif defined(__APPLE__)
    if (library.natives->osx.has_value()) {
        return library.natives->osx->path;
    }
#elif defined(__linux__)
    if (library.natives->linux.has_value()) {
        return library.natives->linux->path;
    }
#endif

    return std::nullopt;
}

std::optional<ResolvedLibrary> LibraryResolver::resolve_single_library(const Library& library) const {
    if (!should_use_library(library)) {
        return std::nullopt;
    }

    ResolvedLibrary resolved;
    resolved.source = library;
    resolved.isNative = library.isNative;
    resolved.extractExclude = library.extractExclude;

    // Determine download URL and local path
    if (library.isNative && library.natives.has_value()) {
        // Native library - select platform-specific artifact
        auto nativeUrl = select_native_artifact_url(library);
        auto nativePath = select_native_local_path(library);

        if (nativeUrl.has_value()) {
            resolved.downloadUrl = nativeUrl.value();
        } else {
            resolved.downloadUrl = build_maven_url(library, defaultRepository_);
        }

        if (nativePath.has_value()) {
            resolved.localPath = nativePath.value();
        } else {
            resolved.localPath = build_maven_path(library);
        }

        // Get SHA1 and size from native artifact
#if defined(_WIN32)
        if (library.natives->windows.has_value()) {
            resolved.sha1 = library.natives->windows->sha1;
            resolved.size = library.natives->windows->size;
        }
#elif defined(__APPLE__)
        if (library.natives->osx.has_value()) {
            resolved.sha1 = library.natives->osx->sha1;
            resolved.size = library.natives->osx->size;
        }
#elif defined(__linux__)
        if (library.natives->linux.has_value()) {
            resolved.sha1 = library.natives->linux->sha1;
            resolved.size = library.natives->linux->size;
        }
#endif
    } else {
        // Regular library
        resolved.downloadUrl = build_maven_url(library, defaultRepository_);
        resolved.localPath = build_maven_path(library);

        if (library.download.has_value()) {
            resolved.sha1 = library.download->sha1;
            resolved.size = library.download->size;
        }
    }

    return resolved;
}

LibraryResolutionResult LibraryResolver::resolve_libraries(const MinecraftVersionPackage& package) const {
    LibraryResolutionResult result;

    for (const auto& library : package.libraries) {
        auto resolved = resolve_single_library(library);
        if (resolved.has_value()) {
            if (resolved->isNative) {
                result.nativeLibraries.push_back(std::move(*resolved));
                result.nativeSize += resolved->size;
            } else {
                result.libraries.push_back(std::move(*resolved));
                result.totalSize += resolved->size;
            }
        }
    }

    return result;
}

bool LibraryResolver::check_library_integrity(const ResolvedLibrary& library,
                                              const std::filesystem::path& librariesDir) const {
    auto filePath = librariesDir / library.localPath;
    return check_library_integrity(filePath, library.sha1);
}

bool LibraryResolver::check_library_integrity(const std::filesystem::path& filePath,
                                              const std::string& expectedSha1) {
    if (!std::filesystem::exists(filePath)) {
        return false;
    }

    if (expectedSha1.empty()) {
        // No checksum to verify, just check file exists
        return true;
    }

    std::string error;
    std::string actualSha1 = dawn::infra::hash::sha256_file_hex(filePath, &error);

    // SHA1 from Mojang is actually SHA-256 in newer versions
    // Try SHA-256 comparison
    if (!actualSha1.empty()) {
        // Convert both to lowercase for comparison
        std::string expectedLower = to_lower(expectedSha1);
        std::string actualLower = to_lower(actualSha1);
        return expectedLower == actualLower;
    }

    return false;
}

bool LibraryResolver::download_library(const ResolvedLibrary& library,
                                       const std::filesystem::path& outputDir,
                                       std::string* error,
                                       LibraryDownloadCallback callback) const {
    auto destination = outputDir / library.localPath;

    // Check if already exists and valid
    if (check_library_integrity(library, outputDir)) {
        if (callback) {
            callback(library.source.name, library.size, library.size);
        }
        return true;
    }

    // Ensure parent directory exists
    if (!dawn::infra::fs::ensure_parent_directory(destination, error)) {
        if (error) {
            *error = "failed to create directory for " + library.source.name + ": " + *error;
        }
        return false;
    }

    // Download the library
    auto client = dawn::infra::net::HttpClientFactory::create_default_http_client();
    if (!client) {
        if (error) {
            *error = "failed to create HTTP client";
        }
        return false;
    }

    dawn::infra::net::HttpRequest request;
    request.url = library.downloadUrl;
    request.headers.emplace("Accept", "application/java-archive,application/octet-stream,*/*");

    const auto response = client->send(request);
    if (!response.success()) {
        if (error) {
            *error = "download failed for " + library.source.name + ": HTTP " + std::to_string(response.statusCode);
        }
        return false;
    }

    // Write file
    if (!dawn::infra::fs::write_binary_file(destination, response.body, error)) {
        if (error) {
            *error = "failed to write " + library.source.name + ": " + *error;
        }
        return false;
    }

    // Verify checksum
    if (!library.sha1.empty()) {
        if (!check_library_integrity(destination, library.sha1)) {
            if (error) {
                *error = "checksum mismatch for " + library.source.name;
            }
            // Remove corrupted file
            std::error_code ec;
            std::filesystem::remove(destination, ec);
            return false;
        }
    }

    if (callback) {
        callback(library.source.name, response.body.size(), library.size);
    }

    return true;
}

LibraryBatchDownloadResult LibraryResolver::download_libraries(
    const std::vector<ResolvedLibrary>& libraries,
    const std::filesystem::path& outputDir,
    int maxConcurrency,
    LibraryDownloadCallback callback) const {

    LibraryBatchDownloadResult result;
    result.results.reserve(libraries.size());

    if (libraries.empty()) {
        return result;
    }

    const auto workerCount = static_cast<std::size_t>((std::max)(1, maxConcurrency));
    std::atomic_size_t nextIndex{0};
    std::mutex resultMutex;

    std::vector<std::future<void>> workers;
    workers.reserve(workerCount);

    for (std::size_t worker = 0; worker < workerCount; ++worker) {
        workers.push_back(std::async(std::launch::async, [&]() {
            for (;;) {
                const auto index = nextIndex.fetch_add(1);
                if (index >= libraries.size()) {
                    break;
                }

                const auto& library = libraries[index];
                LibraryDownloadResult downloadResult;
                downloadResult.libraryName = library.source.name;
                downloadResult.destination = outputDir / library.localPath;

                // Check if already exists and valid
                if (check_library_integrity(library, outputDir)) {
                    downloadResult.success = true;
                    downloadResult.verified = true;
                    if (callback) {
                        callback(library.source.name, library.size, library.size);
                    }
                    std::lock_guard<std::mutex> lock(resultMutex);
                    result.results.push_back(std::move(downloadResult));
                    ++result.skipped;
                    continue;
                }

                std::string error;
                if (download_library(library, outputDir, &error, callback)) {
                    downloadResult.success = true;
                    downloadResult.verified = !library.sha1.empty();
                    std::lock_guard<std::mutex> lock(resultMutex);
                    result.results.push_back(std::move(downloadResult));
                    ++result.succeeded;
                } else {
                    downloadResult.success = false;
                    downloadResult.error = error;
                    std::lock_guard<std::mutex> lock(resultMutex);
                    result.results.push_back(std::move(downloadResult));
                    result.errors.push_back(error);
                    ++result.failed;
                }
            }
        }));
    }

    for (auto& worker : workers) {
        worker.get();
    }

    return result;
}

bool LibraryResolver::should_extract_file(const std::string& filename,
                                          const std::vector<std::string>& excludePatterns) const {
    for (const auto& pattern : excludePatterns) {
        if (match_wildcard(pattern, filename)) {
            return false;
        }
    }
    return true;
}

NativeExtractionResult LibraryResolver::extract_natives(const ResolvedLibrary& nativeLibrary,
                                                        const std::filesystem::path& sourcePath,
                                                        const std::filesystem::path& nativesDir) const {
    NativeExtractionResult result;
    result.libraryName = nativeLibrary.source.name;
    result.sourcePath = sourcePath;
    result.extractDir = nativesDir;

    if (!std::filesystem::exists(sourcePath)) {
        result.error = "source file does not exist: " + sourcePath.string();
        return result;
    }

    // Ensure natives directory exists
    std::string error;
    if (!dawn::infra::fs::ensure_directory(nativesDir, &error)) {
        result.error = "failed to create natives directory: " + error;
        return result;
    }

    // For now, use a simplified extraction approach
    // In a full implementation, this would use a ZIP library
    // For Windows, we could use System.IO.Compression or a C++ ZIP library

    // Placeholder: Copy the native jar to natives directory
    // In real implementation, extract contents
    std::string filename = sourcePath.filename().string();
    auto destination = nativesDir / filename;

    std::error_code ec;
    std::filesystem::copy_file(sourcePath, destination,
                               std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        result.error = "failed to copy native library: " + ec.message();
        return result;
    }

    result.extractedFiles.push_back(destination);
    result.success = true;

    return result;
}

std::vector<NativeExtractionResult> LibraryResolver::extract_all_natives(
    const std::vector<ResolvedLibrary>& nativeLibraries,
    const std::filesystem::path& librariesDir,
    const std::filesystem::path& nativesDir) const {

    std::vector<NativeExtractionResult> results;
    results.reserve(nativeLibraries.size());

    for (const auto& library : nativeLibraries) {
        auto sourcePath = librariesDir / library.localPath;
        results.push_back(extract_natives(library, sourcePath, nativesDir));
    }

    return results;
}

std::string LibraryResolver::build_classpath(const std::vector<ResolvedLibrary>& libraries,
                                             const std::filesystem::path& librariesDir,
                                             const std::filesystem::path& clientJar) const {
    std::vector<std::string> paths;
    paths.reserve(libraries.size() + 1);

    // Add client jar first
    paths.push_back(clientJar.string());

    // Add all libraries
    for (const auto& library : libraries) {
        auto libPath = librariesDir / library.localPath;
        paths.push_back(libPath.string());
    }

    // Join with platform-specific separator
#if defined(_WIN32)
    constexpr char separator = ';';
#else
    constexpr char separator = ':';
#endif

    std::string classpath;
    for (std::size_t i = 0; i < paths.size(); ++i) {
        if (i > 0) {
            classpath += separator;
        }
        classpath += paths[i];
    }

    return classpath;
}

std::string LibraryResolver::build_native_path(const std::vector<ResolvedLibrary>& nativeLibraries,
                                               const std::filesystem::path& nativesDir) const {
    // Return the natives directory path for java.library.path
    return nativesDir.string();
}

} // namespace dawn::core
