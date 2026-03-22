#pragma once

#include "dawn/core/model/enums.h"

#include <filesystem>
#include <string>
#include <vector>

namespace dawn::core {

struct InstanceManifest {
    std::string id;
    std::string name;
    std::string icon;
    std::string mcVersion;
    LoaderType loaderType = LoaderType::None;
    std::string loaderVersion;
    std::string optifineVersion;
    std::string javaProfileId;
    std::string memoryProfile;
    std::string gameDir;
    std::string createdAt;
    std::string lastPlayedAt;
    std::vector<std::string> tags;
    std::string notes;
    std::string themeColor;
};

} // namespace dawn::core
