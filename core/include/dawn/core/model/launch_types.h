#pragma once

#include "dawn/core/model/instance_manifest.h"

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace dawn::core {

struct LaunchCommand {
    std::string executable;
    std::vector<std::string> arguments;
    std::map<std::string, std::string> environment;
    std::filesystem::path workingDirectory;
    bool requiresConsole = false;
};

struct LaunchRequest {
    std::string instanceId;
    InstanceManifest manifest;
    bool dryRun = false;
};

} // namespace dawn::core
