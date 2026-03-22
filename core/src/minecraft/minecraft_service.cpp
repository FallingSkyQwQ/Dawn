#include "dawn/core/minecraft/minecraft_service.h"

#include <algorithm>

namespace dawn::core {

std::vector<MinecraftVersionInfo> MinecraftService::list_versions() const {
    return {
        {"1.20.1", "release", "2023-06-12", 17, "modern release"},
        {"1.19.4", "release", "2023-03-14", 17, "stable baseline"},
        {"23w51b", "snapshot", "2023-12-19", 17, "snapshot sample"},
        {"b1.7.3", "old-beta", "2011-09-14", 8, "legacy compatibility"},
    };
}

MinecraftVersionInfo MinecraftService::find_version(const std::string& versionId) const {
    const auto versions = list_versions();
    const auto it = std::find_if(versions.begin(), versions.end(), [&](const MinecraftVersionInfo& version) {
        return version.versionId == versionId;
    });
    if (it != versions.end()) {
        return *it;
    }
    return MinecraftVersionInfo{versionId, classify(versionId), "", 17, "stub version"};
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
