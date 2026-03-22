#pragma once

#include "dawn/core/model/instance_manifest.h"

#include <string>
#include <vector>

namespace dawn::core {

struct InstanceWorkbenchTab {
    std::string id;
    std::string title;
    std::string summary;
    bool expert = false;
};

struct InstanceWorkbenchState {
    std::string instanceId;
    std::string instanceName;
    std::string selectedTabId;
    std::vector<InstanceWorkbenchTab> tabs;
};

InstanceWorkbenchState build_instance_workbench(const InstanceManifest& manifest);

} // namespace dawn::core
