#pragma once

#include "dawn/core/model/launch_types.h"
#include "dawn/core/model/preflight.h"

namespace dawn::core {

class ILauncherRuntime {
public:
    virtual ~ILauncherRuntime() = default;

    virtual PreflightResult preflight(const LaunchRequest& request) const = 0;
    virtual LaunchCommand buildCommand(const LaunchRequest& request) const = 0;
};

} // namespace dawn::core
