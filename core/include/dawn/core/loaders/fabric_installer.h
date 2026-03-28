#pragma once

#include "dawn/core/interfaces/loader_installer.h"

#include <memory>
#include <string>
#include <vector>

namespace dawn::core {

class FabricInstaller : public ILoaderInstaller {
public:
    FabricInstaller();
    ~FabricInstaller() override;

    std::vector<LoaderVersion> listVersions(const std::string& mcVersion) override;
    TaskPlan buildInstallPlan(const LoaderInstallRequest& request) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace dawn::core
