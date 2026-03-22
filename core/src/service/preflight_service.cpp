#include "dawn/core/service/preflight_service.h"

namespace dawn::core {

PreflightResult PreflightService::inspect(const InstanceManifest& manifest) const {
    PreflightResult result;

    if (manifest.name.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_name", "instance name is required", "set a readable instance name"});
    }
    if (manifest.mcVersion.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_mc_version", "minecraft version is required", "select a release or snapshot"});
    }
    if (manifest.gameDir.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_game_dir", "game directory is required", "assign an instance directory before launch"});
    }
    if (manifest.javaProfileId.empty()) {
        result.issues.push_back({PreflightSeverity::Warning, "missing_java_profile", "java profile is not configured", "pick a bundled or discovered java runtime"});
        result.recommendations.push_back("default to Java 17 for modern releases");
    }
    if (manifest.loaderType == LoaderType::OptiFine && manifest.optifineVersion.empty()) {
        result.issues.push_back({PreflightSeverity::Warning, "missing_optifine_version", "optifine build is not pinned", "record the exact OptiFine build for rollback"});
    }

    result.ready = true;
    for (const auto& issue : result.issues) {
        if (issue.severity == PreflightSeverity::Error) {
            result.ready = false;
            break;
        }
    }
    return result;
}

} // namespace dawn::core
