#pragma once

#include "dawn/core/interfaces/loader_installer.h"

namespace dawn::core {

class OptifineAdapter final : public ILoaderInstaller {
public:
    std::vector<LoaderVersion> listVersions(const std::string& mcVersion) override;
    TaskPlan buildInstallPlan(const LoaderInstallRequest& request) override;
};

} // namespace dawn::core
