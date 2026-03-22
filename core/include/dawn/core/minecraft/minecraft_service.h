#pragma once

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

class MinecraftService {
public:
    std::vector<MinecraftVersionInfo> list_versions() const;
    MinecraftVersionInfo find_version(const std::string& versionId) const;
    std::string classify(const std::string& versionId) const;
};

} // namespace dawn::core
