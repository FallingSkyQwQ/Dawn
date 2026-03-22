#pragma once

#include <string_view>

namespace dawn::core {

enum class LoaderType {
    None,
    Fabric,
    Quilt,
    Forge,
    NeoForge,
    OptiFine,
};

enum class ProjectType {
    Mod,
    Modpack,
    Resourcepack,
    Shader,
};

enum class TaskStatus {
    Pending,
    Running,
    Paused,
    Succeeded,
    Failed,
    Cancelled,
};

enum class PreflightSeverity {
    Info,
    Warning,
    Error,
};

inline std::string_view to_string(LoaderType type) {
    switch (type) {
    case LoaderType::None: return "none";
    case LoaderType::Fabric: return "fabric";
    case LoaderType::Quilt: return "quilt";
    case LoaderType::Forge: return "forge";
    case LoaderType::NeoForge: return "neoforge";
    case LoaderType::OptiFine: return "optifine";
    }
    return "none";
}

inline std::string_view to_string(ProjectType type) {
    switch (type) {
    case ProjectType::Mod: return "mod";
    case ProjectType::Modpack: return "modpack";
    case ProjectType::Resourcepack: return "resourcepack";
    case ProjectType::Shader: return "shader";
    }
    return "mod";
}

inline std::string_view to_string(TaskStatus status) {
    switch (status) {
    case TaskStatus::Pending: return "pending";
    case TaskStatus::Running: return "running";
    case TaskStatus::Paused: return "paused";
    case TaskStatus::Succeeded: return "succeeded";
    case TaskStatus::Failed: return "failed";
    case TaskStatus::Cancelled: return "cancelled";
    }
    return "pending";
}

inline std::string_view to_string(PreflightSeverity severity) {
    switch (severity) {
    case PreflightSeverity::Info: return "info";
    case PreflightSeverity::Warning: return "warning";
    case PreflightSeverity::Error: return "error";
    }
    return "info";
}

inline LoaderType loader_type_from_string(std::string_view text) {
    if (text == "fabric") return LoaderType::Fabric;
    if (text == "quilt") return LoaderType::Quilt;
    if (text == "forge") return LoaderType::Forge;
    if (text == "neoforge") return LoaderType::NeoForge;
    if (text == "optifine") return LoaderType::OptiFine;
    return LoaderType::None;
}

inline ProjectType project_type_from_string(std::string_view text) {
    if (text == "modpack") return ProjectType::Modpack;
    if (text == "resourcepack") return ProjectType::Resourcepack;
    if (text == "shader") return ProjectType::Shader;
    return ProjectType::Mod;
}

inline TaskStatus task_status_from_string(std::string_view text) {
    if (text == "running") return TaskStatus::Running;
    if (text == "paused") return TaskStatus::Paused;
    if (text == "succeeded") return TaskStatus::Succeeded;
    if (text == "failed") return TaskStatus::Failed;
    if (text == "cancelled") return TaskStatus::Cancelled;
    return TaskStatus::Pending;
}

inline PreflightSeverity preflight_severity_from_string(std::string_view text) {
    if (text == "warning") return PreflightSeverity::Warning;
    if (text == "error") return PreflightSeverity::Error;
    return PreflightSeverity::Info;
}

inline bool is_terminal(TaskStatus status) {
    return status == TaskStatus::Succeeded || status == TaskStatus::Failed || status == TaskStatus::Cancelled;
}

} // namespace dawn::core
