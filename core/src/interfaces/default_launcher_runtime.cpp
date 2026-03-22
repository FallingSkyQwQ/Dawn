#include "dawn/core/interfaces/default_launcher_runtime.h"

#include <algorithm>

namespace dawn::core {

PreflightResult DefaultLauncherRuntime::preflight(const LaunchRequest& request) const {
    PreflightResult result;
    const auto& manifest = request.manifest;

    if (manifest.id.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_instance_id", "instance id is empty", "create or load an instance manifest"});
    }
    if (manifest.name.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_name", "instance name is empty", "name the instance before launch"});
    }
    if (manifest.mcVersion.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_mc_version", "minecraft version is empty", "select a minecraft version"});
    }
    if (manifest.gameDir.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_game_dir", "game directory is empty", "set a valid instance directory"});
    }
    if (manifest.javaProfileId.empty()) {
        result.issues.push_back({PreflightSeverity::Warning, "missing_java_profile", "java profile is not configured", "pick a java runtime profile"});
    }
    if (manifest.loaderType == LoaderType::OptiFine && manifest.optifineVersion.empty()) {
        result.issues.push_back({PreflightSeverity::Warning, "missing_optifine_version", "OptiFine version is not pinned", "record the exact OptiFine build for rollback"});
    }
    if (manifest.loaderType == LoaderType::None) {
        result.recommendations.push_back("vanilla instance detected; no loader bootstrap is required");
    }

    result.ready = std::none_of(result.issues.begin(), result.issues.end(), [](const PreflightIssue& issue) {
        return issue.severity == PreflightSeverity::Error;
    });
    return result;
}

LaunchCommand DefaultLauncherRuntime::buildCommand(const LaunchRequest& request) const {
    LaunchCommand command;
    command.executable = "java";
    command.requiresConsole = true;
    command.workingDirectory = request.manifest.gameDir.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(request.manifest.gameDir);
    command.arguments = {
        request.manifest.memoryProfile.empty() ? std::string("-Xmx2G") : ("-Xmx" + request.manifest.memoryProfile),
        "-Djava.awt.headless=false",
        "-Duser.language=en",
        "-Duser.region=US",
        "net.minecraft.client.main.Main"
    };
    command.environment.emplace("DAWN_INSTANCE_ID", request.instanceId);
    command.environment.emplace("DAWN_MINECRAFT_VERSION", request.manifest.mcVersion);
    return command;
}

} // namespace dawn::core
