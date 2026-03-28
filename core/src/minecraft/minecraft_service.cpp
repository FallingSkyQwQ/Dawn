#include "dawn/core/minecraft/minecraft_service.h"

#include "dawn/core/minecraft/library_resolver.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"
#include "dawn/infra/json/simple_json.h"
#include "dawn/infra/net/http_client_factory.h"

#include <algorithm>
#include <array>
#include <memory>
#include <string_view>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;
using dawn::infra::net::HttpRequest;

constexpr std::string_view kVersionManifestUrl = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json";
constexpr std::string_view kDefaultLibrariesDir = "libraries";
constexpr std::string_view kDefaultVersionsDir = "versions";

std::string read_string_field(const Value::Object& object, const std::string& key) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_string()) {
        return value->as_string();
    }
    return {};
}

int default_java_major_for_channel(const std::string& channel) {
    if (channel == "old-alpha" || channel == "old-beta") {
        return 8;
    }
    return 17;
}

std::string normalize_channel(std::string type) {
    if (type == "old_alpha") {
        return "old-alpha";
    }
    if (type == "old_beta") {
        return "old-beta";
    }
    if (type.empty()) {
        return "unknown";
    }
    return type;
}

std::vector<MinecraftVersionInfo> fallback_versions() {
    return {
        {"1.20.1", "release", "2023-06-12", 17, "fallback cached release"},
        {"1.19.4", "release", "2023-03-14", 17, "fallback stable baseline"},
        {"23w51b", "snapshot", "2023-12-19", 17, "fallback snapshot sample"},
        {"b1.7.3", "old-beta", "2011-09-14", 8, "fallback legacy compatibility"},
    };
}

std::vector<MinecraftVersionInfo> fetch_versions_from_manifest() {
    const auto client = dawn::infra::net::HttpClientFactory::create_default_http_client();
    if (!client) {
        return fallback_versions();
    }

    HttpRequest request;
    request.url = std::string(kVersionManifestUrl);
    request.headers.emplace("Accept", "application/json");
    const auto response = client->send(request);
    if (!response.success()) {
        return fallback_versions();
    }

    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        return fallback_versions();
    }

    const auto* versions = dawn::infra::json::find(parsed.value.as_object(), "versions");
    if (!versions || !versions->is_array()) {
        return fallback_versions();
    }

    std::vector<MinecraftVersionInfo> result;
    result.reserve(versions->as_array().size());
    for (const auto& entry : versions->as_array()) {
        if (!entry.is_object()) {
            continue;
        }
        const auto& object = entry.as_object();
        const auto id = read_string_field(object, "id");
        if (id.empty()) {
            continue;
        }
        const auto channel = normalize_channel(read_string_field(object, "type"));
        auto releaseDate = read_string_field(object, "releaseTime");
        if (releaseDate.size() >= 10) {
            releaseDate = releaseDate.substr(0, 10);
        }

        MinecraftVersionInfo info;
        info.versionId = id;
        info.channel = channel;
        info.releaseDate = releaseDate;
        info.recommendedJavaMajor = default_java_major_for_channel(channel);
        info.notes = "official manifest";
        result.push_back(std::move(info));
    }

    if (result.empty()) {
        return fallback_versions();
    }
    return result;
}

std::optional<std::string> find_version_url_in_manifest(const std::string& versionId) {
    const auto client = dawn::infra::net::HttpClientFactory::create_default_http_client();
    if (!client) {
        return std::nullopt;
    }

    HttpRequest request;
    request.url = std::string(kVersionManifestUrl);
    request.headers.emplace("Accept", "application/json");
    const auto response = client->send(request);
    if (!response.success()) {
        return std::nullopt;
    }

    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        return std::nullopt;
    }

    const auto* versions = dawn::infra::json::find(parsed.value.as_object(), "versions");
    if (!versions || !versions->is_array()) {
        return std::nullopt;
    }

    for (const auto& entry : versions->as_array()) {
        if (!entry.is_object()) {
            continue;
        }
        const auto& object = entry.as_object();
        const auto id = read_string_field(object, "id");
        if (id == versionId) {
            return read_string_field(object, "url");
        }
    }

    return std::nullopt;
}

} // namespace

std::vector<MinecraftVersionInfo> MinecraftService::list_versions() const {
    return fetch_versions_from_manifest();
}

MinecraftVersionInfo MinecraftService::find_version(const std::string& versionId) const {
    const auto versions = list_versions();
    const auto it = std::find_if(versions.begin(), versions.end(), [&](const MinecraftVersionInfo& version) {
        return version.versionId == versionId;
    });
    if (it != versions.end()) {
        return *it;
    }
    return MinecraftVersionInfo{versionId, classify(versionId), "", default_java_major_for_channel(classify(versionId)), "unresolved version id"};
}

std::string MinecraftService::classify(const std::string& versionId) const {
    if (versionId.empty()) {
        return "unknown";
    }
    if (versionId.find("snapshot") != std::string::npos || versionId.find('w') != std::string::npos) {
        return "snapshot";
    }
    if (!versionId.empty() && versionId[0] == 'b') {
        return "old-beta";
    }
    if (!versionId.empty() && versionId[0] == 'a') {
        return "old-alpha";
    }
    return "release";
}

std::string MinecraftService::version_manifest_url() {
    return std::string(kVersionManifestUrl);
}

std::filesystem::path MinecraftService::default_libraries_directory() {
    return std::filesystem::path(kDefaultLibrariesDir);
}

std::filesystem::path MinecraftService::default_versions_directory() {
    return std::filesystem::path(kDefaultVersionsDir);
}

bool MinecraftService::fetch_version_package(
    const std::string& versionId,
    MinecraftVersionPackage* package,
    std::string* error) const {

    if (!package) {
        if (error) {
            *error = "package pointer is null";
        }
        return false;
    }

    // Find version URL from manifest
    auto versionUrl = find_version_url_in_manifest(versionId);
    if (!versionUrl.has_value()) {
        if (error) {
            *error = "version not found in manifest: " + versionId;
        }
        return false;
    }

    // Download version JSON
    const auto client = dawn::infra::net::HttpClientFactory::create_default_http_client();
    if (!client) {
        if (error) {
            *error = "failed to create HTTP client";
        }
        return false;
    }

    HttpRequest request;
    request.url = versionUrl.value();
    request.headers.emplace("Accept", "application/json");
    const auto response = client->send(request);
    if (!response.success()) {
        if (error) {
            *error = "failed to download version package: HTTP " + std::to_string(response.statusCode);
        }
        return false;
    }

    // Parse version package
    if (!parse_version_package(response.body, package, error)) {
        return false;
    }

    return true;
}

bool MinecraftService::download_client_jar(
    const MinecraftVersionPackage& package,
    const std::filesystem::path& outputPath,
    std::string* error,
    VersionDownloadCallback callback) const {

    if (!package.downloads.client.has_value()) {
        if (error) {
            *error = "no client download available for version " + package.versionId;
        }
        return false;
    }

    const auto& clientDownload = package.downloads.client.value();

    // Check if already exists and valid
    if (std::filesystem::exists(outputPath)) {
        std::string hashError;
        auto actualHash = dawn::infra::hash::sha256_file_hex(outputPath, &hashError);
        if (hashError.empty() && dawn::infra::hash::compare_hash(clientDownload.sha1, actualHash)) {
            if (callback) {
                callback("client", clientDownload.size, clientDownload.size, "client jar already exists");
            }
            return true;
        }
    }

    // Ensure parent directory exists
    if (!dawn::infra::fs::ensure_parent_directory(outputPath, error)) {
        if (error) {
            *error = "failed to create directory: " + *error;
        }
        return false;
    }

    // Download client jar
    const auto client = dawn::infra::net::HttpClientFactory::create_default_http_client();
    if (!client) {
        if (error) {
            *error = "failed to create HTTP client";
        }
        return false;
    }

    if (callback) {
        callback("client", 0, clientDownload.size, "downloading client jar");
    }

    HttpRequest request;
    request.url = clientDownload.url;
    request.headers.emplace("Accept", "application/java-archive,application/octet-stream,*/*");
    const auto response = client->send(request);
    if (!response.success()) {
        if (error) {
            *error = "failed to download client jar: HTTP " + std::to_string(response.statusCode);
        }
        return false;
    }

    // Write file
    if (!dawn::infra::fs::write_binary_file(outputPath, response.body, error)) {
        if (error) {
            *error = "failed to write client jar: " + *error;
        }
        return false;
    }

    // Verify checksum
    if (!clientDownload.sha1.empty()) {
        std::string hashError;
        auto actualHash = dawn::infra::hash::sha256_file_hex(outputPath, &hashError);
        if (!hashError.empty() || !dawn::infra::hash::compare_hash(clientDownload.sha1, actualHash)) {
            if (error) {
                *error = "client jar checksum mismatch";
            }
            // Remove corrupted file
            std::error_code ec;
            std::filesystem::remove(outputPath, ec);
            return false;
        }
    }

    if (callback) {
        callback("client", response.body.size(), clientDownload.size, "client jar downloaded");
    }

    return true;
}

bool MinecraftService::download_version(
    const std::string& versionId,
    const std::filesystem::path& outputDir,
    VersionDownloadResult* result,
    VersionDownloadCallback callback,
    int maxConcurrency) const {

    if (!result) {
        return false;
    }

    result->versionId = versionId;
    result->success = false;

    // Setup paths
    auto versionsDir = outputDir / kDefaultVersionsDir;
    auto librariesDir = outputDir / kDefaultLibrariesDir;
    auto versionDir = versionsDir / versionId;
    auto nativesDir = versionDir / "natives";

    result->librariesDir = librariesDir;
    result->nativesDir = nativesDir;

    // Step 1: Fetch version package
    if (callback) {
        callback("metadata", 0, 1, "fetching version metadata");
    }

    MinecraftVersionPackage package;
    std::string error;
    if (!fetch_version_package(versionId, &package, &error)) {
        result->errors.push_back(error);
        return false;
    }

    result->package = package;

    if (callback) {
        callback("metadata", 1, 1, "version metadata fetched");
    }

    // Step 2: Save version JSON
    if (!dawn::infra::fs::ensure_directory(versionDir, &error)) {
        result->errors.push_back("failed to create version directory: " + error);
        return false;
    }

    auto versionJsonPath = versionDir / (versionId + ".json");
    result->versionJsonPath = versionJsonPath;

    // Re-download version JSON to save it
    auto versionUrl = find_version_url_in_manifest(versionId);
    if (versionUrl.has_value()) {
        const auto client = dawn::infra::net::HttpClientFactory::create_default_http_client();
        if (client) {
            HttpRequest request;
            request.url = versionUrl.value();
            request.headers.emplace("Accept", "application/json");
            const auto response = client->send(request);
            if (response.success()) {
                dawn::infra::fs::write_text_file(versionJsonPath, response.body, nullptr);
            }
        }
    }

    // Step 3: Download client jar
    auto clientJarPath = versionDir / (versionId + ".jar");
    result->clientJarPath = clientJarPath;

    if (!download_client_jar(package, clientJarPath, &error, callback)) {
        result->errors.push_back(error);
        // Continue even if client jar fails (might be already downloaded)
    }

    // Step 4: Resolve and download libraries
    if (callback) {
        callback("libraries", 0, package.libraries.size(), "resolving libraries");
    }

    LibraryResolver resolver;
    auto resolutionResult = resolver.resolve_libraries(package);

    if (!resolutionResult.errors.empty()) {
        result->errors.insert(result->errors.end(),
                              resolutionResult.errors.begin(),
                              resolutionResult.errors.end());
    }

    // Download regular libraries
    if (!resolutionResult.libraries.empty()) {
        auto libCallback = [&](const std::string& libraryName, std::size_t downloaded, std::size_t total) {
            if (callback) {
                callback("libraries", downloaded, total, "downloading " + libraryName);
            }
        };

        auto downloadResult = resolver.download_libraries(
            resolutionResult.libraries,
            librariesDir,
            maxConcurrency,
            libCallback);

        result->librariesDownloaded = downloadResult.succeeded;
        result->librariesSkipped = downloadResult.skipped;
        result->librariesFailed = downloadResult.failed;

        if (!downloadResult.errors.empty()) {
            result->errors.insert(result->errors.end(),
                                  downloadResult.errors.begin(),
                                  downloadResult.errors.end());
        }
    }

    // Download and extract native libraries
    if (!resolutionResult.nativeLibraries.empty()) {
        auto nativeCallback = [&](const std::string& libraryName, std::size_t downloaded, std::size_t total) {
            if (callback) {
                callback("libraries", downloaded, total, "downloading native " + libraryName);
            }
        };

        auto nativeResult = resolver.download_libraries(
            resolutionResult.nativeLibraries,
            librariesDir,
            maxConcurrency,
            nativeCallback);

        result->librariesDownloaded += nativeResult.succeeded;
        result->librariesSkipped += nativeResult.skipped;
        result->librariesFailed += nativeResult.failed;

        if (!nativeResult.errors.empty()) {
            result->errors.insert(result->errors.end(),
                                  nativeResult.errors.begin(),
                                  nativeResult.errors.end());
        }

        // Extract natives
        if (resolver.extract_natives()) {
            auto extractionResults = resolver.extract_all_natives(
                resolutionResult.nativeLibraries,
                librariesDir,
                nativesDir);

            for (const auto& extraction : extractionResults) {
                if (!extraction.success && !extraction.error.empty()) {
                    result->errors.push_back("native extraction failed: " + extraction.error);
                }
            }
        }
    }

    if (callback) {
        callback("libraries", resolutionResult.libraries.size() + resolutionResult.nativeLibraries.size(),
                 resolutionResult.libraries.size() + resolutionResult.nativeLibraries.size(),
                 "libraries complete");
    }

    result->success = result->librariesFailed == 0;
    return result->success;
}

} // namespace dawn::core
