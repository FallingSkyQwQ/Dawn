#pragma once

#include "dawn/core/model/preflight.h"
#include "dawn/core/model/instance_manifest.h"

namespace dawn::core {

class PreflightService {
public:
    PreflightResult inspect(const InstanceManifest& manifest) const;
};

} // namespace dawn::core
