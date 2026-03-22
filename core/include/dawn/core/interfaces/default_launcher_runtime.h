#pragma once

#include "dawn/core/interfaces/launcher_runtime.h"

namespace dawn::core {

class DefaultLauncherRuntime final : public ILauncherRuntime {
public:
    PreflightResult preflight(const LaunchRequest& request) const override;
    LaunchCommand buildCommand(const LaunchRequest& request) const override;
};

} // namespace dawn::core
