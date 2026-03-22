#include "dawn/core/model/instance_workbench.h"

namespace dawn::core {

InstanceWorkbenchState build_instance_workbench(const InstanceManifest& manifest) {
    InstanceWorkbenchState state;
    state.instanceId = manifest.id;
    state.instanceName = manifest.name;
    state.selectedTabId = "overview";
    state.tabs = {
        {"overview", "Overview", "Instance summary, health, and launch readiness.", false},
        {"mods", "Mods", "Installed mod list, enabled state, and dependency status.", false},
        {"resourcepacks", "Resource Packs", "Resource pack ordering and enabled state.", false},
        {"shaderpacks", "Shaders", "Shader pack selection and compatibility notes.", false},
        {"worlds", "Worlds & Saves", "World list, backups, and restore points.", false},
        {"logs", "Logs", "Launch logs, crash logs, and repair hints.", false},
        {"runtime", "Runtime", "Java, loader, arguments, and memory profile.", false},
        {"advanced", "Advanced", "Expert-only settings and override controls.", true},
    };
    return state;
}

} // namespace dawn::core
