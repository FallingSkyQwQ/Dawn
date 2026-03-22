#pragma once

#include "dawn/core/model/content_types.h"
#include "dawn/core/model/task_types.h"

namespace dawn::core {

class ILoaderInstaller {
public:
    virtual ~ILoaderInstaller() = default;

    virtual std::vector<LoaderVersion> listVersions(const std::string& mcVersion) = 0;
    virtual TaskPlan buildInstallPlan(const LoaderInstallRequest& request) = 0;
};

} // namespace dawn::core
