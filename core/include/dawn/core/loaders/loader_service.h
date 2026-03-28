#pragma once

#include "dawn/core/interfaces/loader_installer.h"
#include "dawn/core/model/content_types.h"
#include "dawn/core/model/enums.h"

#include <memory>
#include <string>
#include <vector>

namespace dawn::core {

struct LoaderProfile {
    LoaderType loaderType = LoaderType::None;
    std::string versionId;
    std::string mcVersion;
    std::string notes;
    std::string javaHint;
};

class LoaderService {
public:
    // List available loaders for a Minecraft version
    std::vector<LoaderProfile> list_loaders(const std::string& mcVersion) const;

    // Recommend a loader based on preference
    LoaderProfile recommend_loader(const std::string& mcVersion, LoaderType preferred = LoaderType::None) const;

    // Create an installer for a specific loader type
    std::unique_ptr<ILoaderInstaller> create_installer(LoaderType type);

    // List available versions for a specific loader type
    std::vector<LoaderVersion> list_loader_versions(LoaderType type, const std::string& mcVersion);

    // Build installation plan for a loader
    TaskPlan build_install_plan(const LoaderInstallRequest& request);
};

} // namespace dawn::core
