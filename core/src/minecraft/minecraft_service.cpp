#include "dawn/core/minecraft/minecraft_service.h"

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

} // namespace dawn::core
