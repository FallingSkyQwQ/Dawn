#include "dawn/core/provider/optifine_adapter.h"

namespace dawn::core {

std::vector<LoaderVersion> OptifineAdapter::listVersions(const std::string& mcVersion) {
    return {
        LoaderVersion{
            .versionId = mcVersion + "-OptiFine-HD_U_stub",
            .mcVersion = mcVersion,
            .loaderType = LoaderType::OptiFine,
        }
    };
}

TaskPlan OptifineAdapter::buildInstallPlan(const LoaderInstallRequest& request) {
    TaskPlan plan;
    plan.id = "optifine-" + request.mcVersion;
    plan.title = "Install OptiFine for " + request.mcVersion;
    plan.steps = {
        {"detect", "Detect compatible OptiFine package", TaskStatus::Pending, 0, {}},
        {"stage", "Stage installer payload", TaskStatus::Pending, 0, {}},
        {"apply", "Apply OptiFine to the target instance", TaskStatus::Pending, 0, {}}
    };
    return plan;
}

} // namespace dawn::core
