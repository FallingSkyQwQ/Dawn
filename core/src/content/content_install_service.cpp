#include "dawn/core/content/content_install_service.h"

#include "dawn/core/pipeline/task_pipeline.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/core/serialization/manifest_codec.h"
#include "dawn/infra/fs/file_system.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <system_error>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::fs::ensure_directory;
using dawn::infra::fs::write_text_file;

std::string sanitize_component(const std::string& text) {
    std::string result;
    result.reserve(text.size());
    for (char ch : text) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!result.empty() && result.back() != '-') {
            result.push_back('-');
        }
    }
    if (result.empty()) {
        result = "content";
    }
    return result;
}

std::string timestamp_id() {
    return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
}

void append_unique(std::vector<std::string>& values, const std::string& value) {
    if (std::find(values.begin(), values.end(), value) == values.end()) {
        values.push_back(value);
    }
}

std::string extension_for_project(ProjectType type) {
    switch (type) {
    case ProjectType::Resourcepack:
    case ProjectType::Shader:
        return ".zip";
    case ProjectType::Mod:
        return ".jar";
    case ProjectType::Modpack:
        return ".zip";
    }
    return ".bin";
}

bool contains_loader(const std::vector<LoaderType>& loaders, LoaderType loader) {
    return std::find(loaders.begin(), loaders.end(), loader) != loaders.end();
}

bool has_game_version(const std::vector<std::string>& versions, const std::string& value) {
    return std::find(versions.begin(), versions.end(), value) != versions.end();
}

std::string requirement_to_string(DependencyRequirement requirement) {
    switch (requirement) {
    case DependencyRequirement::Required: return "required";
    case DependencyRequirement::Optional: return "optional";
    case DependencyRequirement::Incompatible: return "incompatible";
    case DependencyRequirement::Embedded: return "embedded";
    }
    return "required";
}

bool version_compatible_with_instance(const ContentVersion& version, const InstanceManifest& instance) {
    const bool loaderSupported = version.loaders.empty()
        || instance.loaderType == LoaderType::None
        || contains_loader(version.loaders, instance.loaderType);
    const bool versionSupported = version.gameVersions.empty()
        || instance.mcVersion.empty()
        || has_game_version(version.gameVersions, instance.mcVersion);
    return loaderSupported && versionSupported;
}

std::string version_compatibility_reason(const ContentVersion& version, const InstanceManifest& instance) {
    const bool loaderSupported = version.loaders.empty()
        || instance.loaderType == LoaderType::None
        || contains_loader(version.loaders, instance.loaderType);
    const bool versionSupported = version.gameVersions.empty()
        || instance.mcVersion.empty()
        || has_game_version(version.gameVersions, instance.mcVersion);
    if (loaderSupported && versionSupported) {
        return "matches instance loader and game version";
    }
    if (!loaderSupported && !versionSupported) {
        return "loader and game version do not match the target instance";
    }
    if (!loaderSupported) {
        return "loader is not compatible with the target instance";
    }
    return "game version is not compatible with the target instance";
}

std::string dependency_status_from_requirement(DependencyRequirement requirement, bool installed, bool conflict) {
    if (conflict) {
        return "conflict";
    }
    if (installed) {
        return "installed";
    }
    switch (requirement) {
    case DependencyRequirement::Required: return "missing";
    case DependencyRequirement::Optional: return "optional";
    case DependencyRequirement::Incompatible: return "incompatible";
    case DependencyRequirement::Embedded: return "embedded";
    }
    return "missing";
}

std::vector<ContentLock> collect_installed_locks(const InstanceManifest& instance) {
    std::vector<ContentLock> locks;
    const std::filesystem::path gameDir = instance.gameDir.empty()
        ? (std::filesystem::path(instance.id.empty() ? "instance" : instance.id) / "game")
        : std::filesystem::path(instance.gameDir);
    const auto root = gameDir / "config" / "dawn" / "content-locks";

    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return locks;
    }

    for (const auto& providerDir : std::filesystem::directory_iterator(root, ec)) {
        if (ec) {
            break;
        }
        if (!providerDir.is_directory()) {
            continue;
        }
        for (const auto& path : dawn::infra::fs::list_files(providerDir.path(), ".json")) {
            ContentLock lock;
            std::string error;
            if (load_content_lock(path, &lock, &error)) {
                locks.push_back(std::move(lock));
            }
        }
    }
    return locks;
}

std::string filename_from_url(const std::string& url) {
    const auto cut = url.find_first_of("?#");
    const auto clean = cut == std::string::npos ? url : url.substr(0, cut);
    const auto filename = std::filesystem::path(clean).filename().string();
    return filename.empty() ? std::string() : filename;
}

TaskPlan make_plan(const InstallRequest& request) {
    TaskPlan plan;
    plan.id = "install-" + sanitize_component(request.provider) + "-" + sanitize_component(request.projectId) + "-" + timestamp_id();
    plan.title = request.projectType == ProjectType::Modpack
        ? "Prepare modpack install for " + request.projectId
        : "Install " + request.projectId;
    if (request.projectType == ProjectType::Modpack) {
        plan.steps = {
            {"create-instance", "Create a new instance", TaskStatus::Pending, 0, {}},
            {"download", "Download pack metadata", TaskStatus::Pending, 0, {}},
            {"deploy", "Deploy pack contents", TaskStatus::Pending, 0, {}},
            {"lock", "Write content lock", TaskStatus::Pending, 0, {}},
        };
        return plan;
    }

    plan.steps = {
        {"resolve", "Resolve dependency graph", TaskStatus::Pending, 0, {}},
        {"download", "Download artifact", TaskStatus::Pending, 0, {}},
        {"deploy", "Deploy into instance", TaskStatus::Pending, 0, {}},
        {"lock", "Write content lock", TaskStatus::Pending, 0, {}},
    };
    return plan;
}

void add_logs(ContentInstallResult* result, const std::vector<std::string>& logs, const std::string& prefix = {}) {
    if (!result) {
        return;
    }
    for (const auto& log : logs) {
        result->logs.push_back(prefix.empty() ? log : prefix + log);
    }
}

void add_rollback_event(ContentInstallResult* result, const std::string& step, const std::string& action, const std::filesystem::path& target, const std::string& status, const std::string& message) {
    if (!result) {
        return;
    }
    result->rollbackEvents.push_back(ContentInstallResult::RollbackEvent{
        step,
        action,
        target.generic_string(),
        status,
        message,
    });
    std::string log = "rollback: step=" + step + ", action=" + action + ", target=" + target.generic_string() + ", status=" + status;
    if (!message.empty()) {
        log += ", message=" + message;
    }
    result->logs.push_back(std::move(log));
}

void cleanup_target(ContentInstallResult* result, const std::string& step, const std::string& action, const std::filesystem::path& target) {
    if (!result) {
        return;
    }
    std::error_code ec;
    const auto removedCount = std::filesystem::remove_all(target, ec);
    if (ec) {
        add_rollback_event(result, step, action, target, "failed", ec.message());
        return;
    }
    if (removedCount == 0) {
        add_rollback_event(result, step, action, target, "skipped", "not present");
        return;
    }
    add_rollback_event(result, step, action, target, "removed", std::to_string(removedCount) + " item(s)");
}

bool mark_queue_step(TaskQueue* queue, const std::string& taskId, const std::string& stepId, TaskStatus status, const std::string& detail) {
    if (!queue) {
        return true;
    }
    return queue->complete_step(taskId, stepId, status, detail);
}

void fail_result(ContentInstallResult* result, TaskPipeline* pipeline, TaskQueue* queue, const std::string& stepId, const std::string& message) {
    if (pipeline) {
        pipeline->advance_step(stepId, TaskStatus::Failed, message);
    }
    if (queue && !result->queuedTaskId.empty()) {
        mark_queue_step(queue, result->queuedTaskId, stepId, TaskStatus::Failed, message);
    }
    if (pipeline) {
        result->plan = pipeline->plan();
        result->taskResult = pipeline->finish(message);
    } else {
        result->taskResult.planId = result->plan.id;
        result->taskResult.status = TaskStatus::Failed;
        result->taskResult.summary = message;
    }
    result->plan.status = TaskStatus::Failed;
    result->success = false;
    result->status = ContentInstallStatus::Failed;
    result->message = message;
    result->logs.push_back(message);
}

} // namespace

ContentInstallService::ContentInstallService(std::filesystem::path root, DownloadService& downloadService)
    : root_(std::move(root))
    , repository_(root_)
    , downloadService_(downloadService) {
}

const std::filesystem::path& ContentInstallService::root() const noexcept {
    return root_;
}

TaskPlan ContentInstallService::build_install_plan(const InstallRequest& request) const {
    return make_plan(request);
}

std::filesystem::path ContentInstallService::deployment_directory_for(ProjectType type, const InstanceManifest& instance) {
    const std::filesystem::path gameDir = instance.gameDir.empty()
        ? (std::filesystem::path(instance.id.empty() ? "instance" : instance.id) / "game")
        : std::filesystem::path(instance.gameDir);

    switch (type) {
    case ProjectType::Mod:
        return gameDir / "mods";
    case ProjectType::Resourcepack:
        return gameDir / "resourcepacks";
    case ProjectType::Shader:
        return gameDir / "shaderpacks";
    case ProjectType::Modpack:
        return gameDir / "modpacks";
    }
    return gameDir / "mods";
}

std::filesystem::path ContentInstallService::lock_path_for(const InstallRequest& request, const InstanceManifest& instance) {
    const std::filesystem::path gameDir = instance.gameDir.empty()
        ? (std::filesystem::path(instance.id.empty() ? "instance" : instance.id) / "game")
        : std::filesystem::path(instance.gameDir);
    return gameDir / "config" / "dawn" / "content-locks" / sanitize_component(provider_name(request)) / (sanitize_component(request.projectId) + ".json");
}

std::optional<InstanceManifest> ContentInstallService::load_instance(const std::string& instanceId, std::string* error) const {
    return repository_.load_instance(instanceId, error);
}

std::string ContentInstallService::make_install_id(const InstallRequest& request) {
    return "install-" + sanitize_component(request.provider) + "-" + sanitize_component(request.projectId) + "-" + timestamp_id();
}

std::string ContentInstallService::make_target_filename(const ContentVersion& version, const InstallRequest& request) {
    std::string filename = filename_from_url(version.fileUrls.empty() ? std::string() : version.fileUrls.front());
    if (filename.empty()) {
        filename = sanitize_component(request.projectId) + "-" + sanitize_component(version.versionId) + extension_for_project(request.projectType);
    }
    return filename;
}

std::filesystem::path ContentInstallService::staging_path_for(const InstanceManifest& instance, const InstallRequest& request, const ContentVersion& version) {
    const std::filesystem::path gameDir = instance.gameDir.empty()
        ? (std::filesystem::path(instance.id.empty() ? "instance" : instance.id) / "game")
        : std::filesystem::path(instance.gameDir);
    return gameDir / ".dawn" / "staging" / sanitize_component(provider_name(request)) / sanitize_component(request.projectId) / sanitize_component(version.versionId) / make_target_filename(version, request);
}

std::filesystem::path ContentInstallService::final_path_for(const InstanceManifest& instance, const InstallRequest& request, const ContentVersion& version) {
    return deployment_directory_for(request.projectType, instance) / make_target_filename(version, request);
}

std::vector<std::string> ContentInstallService::merge_dependencies(const ContentVersion& version, const DependencyGraph& graph) {
    std::vector<std::string> dependencies;
    for (const auto& dependency : version.dependencies) {
        append_unique(dependencies, dependency.projectId);
    }
    for (const auto& dependency : graph.dependencies) {
        append_unique(dependencies, dependency.projectId);
    }
    for (const auto& lock : graph.locks) {
        for (const auto& dependency : lock.dependencies) {
            append_unique(dependencies, dependency);
        }
    }
    return dependencies;
}

DependencyTreeNode ContentInstallService::build_dependency_tree(
    const InstallRequest& request,
    const ContentVersion& version,
    const DependencyGraph& graph,
    const std::vector<ContentLock>& installedLocks,
    bool selectedVersionCompatible) {
    DependencyTreeNode root;
    root.projectId = request.projectId;
    root.versionId = version.versionId;
    root.requirement = DependencyRequirement::Required;
    root.status = selectedVersionCompatible ? "ready" : "blocked";
    root.message = version.name.empty() ? request.projectId : version.name;

    const auto find_installed = [&](const std::string& projectId, std::string* versionId) {
        for (const auto& lock : installedLocks) {
            if (lock.projectId == projectId) {
                if (versionId) {
                    *versionId = lock.versionId;
                }
                return true;
            }
        }
        return false;
    };

    const auto append_child = [&](const ContentDependency& dependency) {
        std::string installedVersion;
        const bool installed = find_installed(dependency.projectId, &installedVersion);
        const bool conflict = dependency.requirement == DependencyRequirement::Incompatible && installed;
        DependencyTreeNode child;
        child.projectId = dependency.projectId;
        child.versionId = installed ? installedVersion : dependency.versionId;
        child.requirement = dependency.requirement;
        child.status = dependency_status_from_requirement(dependency.requirement, installed, conflict);
        child.message = dependency.note.empty() ? requirement_to_string(dependency.requirement) : dependency.note;
        root.children.push_back(std::move(child));
    };

    for (const auto& dependency : graph.dependencies.empty() ? version.dependencies : graph.dependencies) {
        append_child(dependency);
    }

    return root;
}

std::vector<VersionSuggestion> ContentInstallService::build_version_suggestions(
    const InstallRequest& request,
    const InstanceManifest& instance,
    const std::vector<ContentVersion>& versions,
    const ContentVersion& selectedVersion,
    bool selectedVersionCompatible) {
    (void)request;

    struct Candidate {
        VersionSuggestion suggestion;
        int score = 0;
    };

    std::vector<Candidate> candidates;
    candidates.reserve(versions.size());
    for (const auto& version : versions) {
        Candidate candidate;
        candidate.suggestion.versionId = version.versionId;
        candidate.suggestion.name = version.name.empty() ? version.versionId : version.name;
        candidate.suggestion.gameVersions = version.gameVersions;
        candidate.suggestion.loaders = version.loaders;
        candidate.suggestion.recommended = false;

        const bool loaderSupported = version.loaders.empty()
            || instance.loaderType == LoaderType::None
            || contains_loader(version.loaders, instance.loaderType);
        const bool gameVersionSupported = version.gameVersions.empty()
            || instance.mcVersion.empty()
            || has_game_version(version.gameVersions, instance.mcVersion);

        if (loaderSupported) {
            candidate.score += 30;
        }
        if (gameVersionSupported) {
            candidate.score += 30;
        }
        if (loaderSupported && gameVersionSupported) {
            candidate.score += 40;
        }
        if (version.versionId == selectedVersion.versionId) {
            candidate.score += selectedVersionCompatible ? 50 : 5;
            candidate.suggestion.reason = selectedVersionCompatible
                ? "current selection is compatible"
                : "current selection needs adjustment";
        } else if (loaderSupported && gameVersionSupported) {
            candidate.suggestion.reason = "matches target loader and game version";
        } else if (loaderSupported) {
            candidate.suggestion.reason = "loader matches, game version needs a closer fit";
        } else if (gameVersionSupported) {
            candidate.suggestion.reason = "game version matches, loader needs a closer fit";
        } else {
            candidate.suggestion.reason = "no direct compatibility match";
        }

        if (candidate.score > 0) {
            candidates.push_back(std::move(candidate));
        }
    }

    if (candidates.empty()) {
        VersionSuggestion suggestion;
        suggestion.versionId = selectedVersion.versionId;
        suggestion.name = selectedVersion.name.empty() ? selectedVersion.versionId : selectedVersion.name;
        suggestion.reason = "no compatible version candidates found";
        suggestion.recommended = true;
        suggestion.gameVersions = selectedVersion.gameVersions;
        suggestion.loaders = selectedVersion.loaders;
        return {suggestion};
    }

    std::sort(candidates.begin(), candidates.end(), [](const Candidate& lhs, const Candidate& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.suggestion.versionId < rhs.suggestion.versionId;
    });

    std::vector<VersionSuggestion> suggestions;
    for (std::size_t i = 0; i < candidates.size() && i < 4; ++i) {
        candidates[i].suggestion.recommended = i == 0;
        suggestions.push_back(std::move(candidates[i].suggestion));
    }

    if (!selectedVersionCompatible && !selectedVersion.versionId.empty()) {
        const auto it = std::find_if(suggestions.begin(), suggestions.end(), [&](const VersionSuggestion& suggestion) {
            return suggestion.versionId == selectedVersion.versionId;
        });
        if (it == suggestions.end()) {
            VersionSuggestion current;
            current.versionId = selectedVersion.versionId;
            current.name = selectedVersion.name.empty() ? selectedVersion.versionId : selectedVersion.name;
            current.reason = "current selection is not compatible with the target instance";
            current.recommended = false;
            current.gameVersions = selectedVersion.gameVersions;
            current.loaders = selectedVersion.loaders;
            suggestions.push_back(std::move(current));
        }
    }

    return suggestions;
}

TaskPlan ContentInstallService::build_repair_plan(const InstallRequest& request, const DependencyCheckResult& preview) {
    TaskPlan plan;
    plan.id = "repair-" + sanitize_component(request.provider) + "-" + sanitize_component(request.projectId) + "-" + timestamp_id();
    plan.title = "Repair missing dependencies for " + request.projectId;
    plan.steps = {
        {"resolve", "Resolve missing dependencies", TaskStatus::Pending, 0, {}},
        {"download", "Download missing artifacts", TaskStatus::Pending, 0, {}},
        {"install", "Install missing dependencies", TaskStatus::Pending, 0, {}},
    };

    if (!preview.graph.missing.empty()) {
        plan.steps.front().detail = "missing: " + preview.graph.missing.front();
    }
    return plan;
}

std::string ContentInstallService::provider_name(const InstallRequest& request) {
    return request.provider.empty() ? std::string("modrinth") : request.provider;
}

DependencyCheckResult ContentInstallService::preview(const InstallRequest& request, IContentProvider& provider) const {
    DependencyCheckResult result;

    std::string instanceError;
    auto instance = load_instance(request.instanceId, &instanceError);
    if (!instance) {
        result.blocked = true;
        result.graph.conflicts.push_back("instance not found");
        result.diagnostics.push_back({
            "missing_instance",
            PreflightSeverity::Error,
            instanceError.empty() ? "instance not found" : instanceError,
            "select a valid instance",
            true,
        });
        return result;
    }

    const auto versions = provider.versions(request.projectId);
    const auto selected = std::find_if(versions.begin(), versions.end(), [&](const ContentVersion& version) {
        return request.versionId.empty() || version.versionId == request.versionId;
    });
    if (selected == versions.end()) {
        result.blocked = true;
        result.graph.conflicts.push_back("version not found");
        result.diagnostics.push_back({
            "missing_version",
            PreflightSeverity::Error,
            "requested version not found",
            "pick another version",
            true,
        });
        return result;
    }

    result.graph = provider.resolveDependencies(request);
    if (result.graph.dependencies.empty() && !selected->dependencies.empty()) {
        result.graph.dependencies = selected->dependencies;
    }

    const auto installedLocks = collect_installed_locks(*instance);
    const bool loaderSupported = selected->loaders.empty() || instance->loaderType == LoaderType::None || contains_loader(selected->loaders, instance->loaderType);
    if (!loaderSupported) {
        result.blocked = true;
        result.graph.conflicts.push_back("loader:" + std::string(to_string(instance->loaderType)));
        result.diagnostics.push_back({
            "loader_incompatible",
            PreflightSeverity::Error,
            "selected version is not compatible with the instance loader",
            "choose a build that supports " + std::string(to_string(instance->loaderType)),
            true,
        });
    }

    const bool selectedVersionCompatible = version_compatible_with_instance(*selected, *instance);
    result.dependencyTree = build_dependency_tree(request, *selected, result.graph, installedLocks, selectedVersionCompatible);
    result.versionSuggestions = build_version_suggestions(request, *instance, versions, *selected, selectedVersionCompatible);

    auto is_installed = [&](const std::string& projectId, std::string* installedVersion = nullptr) {
        for (const auto& lock : installedLocks) {
            if (lock.projectId == projectId) {
                if (installedVersion) {
                    *installedVersion = lock.versionId;
                }
                return true;
            }
        }
        return false;
    };

    for (const auto& dependency : result.graph.dependencies) {
        std::string installedVersion;
        const bool installed = is_installed(dependency.projectId, &installedVersion);
        switch (dependency.requirement) {
        case DependencyRequirement::Required:
            if (!installed) {
                result.blocked = true;
                result.graph.missing.push_back(dependency.projectId);
                result.diagnostics.push_back({
                    "missing_required_dependency",
                    PreflightSeverity::Error,
                    "required dependency missing: " + dependency.projectId,
                    "install " + dependency.projectId + " before continuing",
                    true,
                });
            }
            break;
        case DependencyRequirement::Optional:
        case DependencyRequirement::Embedded:
            if (!installed) {
                result.graph.optional.push_back(dependency.projectId);
                result.diagnostics.push_back({
                    "optional_dependency_missing",
                    PreflightSeverity::Info,
                    "optional dependency not installed: " + dependency.projectId,
                    "install later if a feature requires it",
                    false,
                });
            }
            break;
        case DependencyRequirement::Incompatible:
            if (installed) {
                result.blocked = true;
                result.graph.conflicts.push_back(dependency.projectId);
                result.diagnostics.push_back({
                    "incompatible_dependency",
                    PreflightSeverity::Error,
                    "incompatible dependency already installed: " + dependency.projectId,
                    "remove the conflicting project before installing this version",
                    true,
                });
            }
            break;
        }
    }

    for (const auto& lock : installedLocks) {
        if (lock.projectId == request.projectId && lock.versionId != selected->versionId) {
            result.blocked = true;
            result.graph.conflicts.push_back(lock.projectId + "@" + lock.versionId);
            result.diagnostics.push_back({
                "project_version_conflict",
                PreflightSeverity::Error,
                "another version of the same project is already installed: " + lock.versionId,
                "remove the previous version or install into a fresh instance",
                true,
            });
        }
    }

    result.dependencyTree.status = result.blocked ? "blocked" : "ready";

    if (result.diagnostics.empty()) {
        result.diagnostics.push_back({
            "install_preview_ok",
            PreflightSeverity::Info,
            "dependency graph looks consistent",
            "proceed with installation",
            false,
        });
    }

    if (!result.graph.missing.empty()) {
        result.repairPlan = build_repair_plan(request, result);
        result.repairPlanAvailable = true;
    }
    return result;
}

ContentInstallResult ContentInstallService::install(const InstallRequest& request, IContentProvider& provider, TaskQueue* queue) const {
    ContentInstallResult result;
    result.plan = build_install_plan(request);

    if (request.projectType == ProjectType::Modpack) {
        result.requiresNewInstance = true;
        result.status = ContentInstallStatus::CreateInstanceRequired;
        result.message = "create instance required";
        result.plan.status = TaskStatus::Paused;
        result.taskResult.planId = result.plan.id;
        result.taskResult.status = TaskStatus::Paused;
        result.taskResult.summary = result.message;
        result.logs.push_back("modpack install requires a new instance");

        if (queue) {
            result.queuedTaskId = queue->enqueue(result.plan);
            if (auto* task = queue->find(result.queuedTaskId)) {
                task->status = TaskStatus::Paused;
                if (!task->steps.empty()) {
                    task->steps.front().status = TaskStatus::Paused;
                    task->steps.front().detail = result.message;
                }
            }
        }
        return result;
    }

    TaskPipeline pipeline(result.plan);
    pipeline.start();
    result.status = ContentInstallStatus::Pending;
    result.logs.push_back("install plan created for " + request.projectId);

    if (queue) {
        result.queuedTaskId = queue->enqueue(result.plan);
        queue->start(result.queuedTaskId);
    }

    const auto dependencyCheck = preview(request, provider);
    result.dependencyCheck = dependencyCheck;
    result.diagnostics = dependencyCheck.diagnostics;
    result.missingDependencies = dependencyCheck.graph.missing;
    result.conflicts = dependencyCheck.graph.conflicts;
    for (const auto& missing : result.missingDependencies) {
        result.logs.push_back("missing dependency: " + missing);
    }
    for (const auto& conflict : result.conflicts) {
        result.logs.push_back("conflict: " + conflict);
    }

    if (dependencyCheck.blocked) {
        result.message = dependencyCheck.diagnostics.empty()
            ? "dependency check blocked installation"
            : dependencyCheck.diagnostics.front().message;
        fail_result(&result, &pipeline, queue, "resolve", result.message);
        add_rollback_event(&result, "resolve", "abort install", std::filesystem::path(request.projectId), "blocked", result.message);
        return result;
    }

    std::string instanceError;
    auto instance = load_instance(request.instanceId, &instanceError);
    if (!instance) {
        result.message = instanceError.empty() ? "instance not found" : instanceError;
        fail_result(&result, &pipeline, queue, "resolve", result.message);
        add_rollback_event(&result, "resolve", "abort install", std::filesystem::path(request.projectId), "blocked", result.message);
        return result;
    }

    const auto versions = provider.versions(request.projectId);
    if (versions.empty()) {
        result.message = "no versions available for project";
        fail_result(&result, &pipeline, queue, "resolve", result.message);
        add_rollback_event(&result, "resolve", "abort install", std::filesystem::path(request.projectId), "blocked", result.message);
        return result;
    }

    auto selected = std::find_if(versions.begin(), versions.end(), [&](const ContentVersion& version) {
        return request.versionId.empty() || version.versionId == request.versionId;
    });
    if (selected == versions.end()) {
        result.message = "requested version not found";
        fail_result(&result, &pipeline, queue, "resolve", result.message);
        add_rollback_event(&result, "resolve", "abort install", std::filesystem::path(request.projectId), "blocked", result.message);
        return result;
    }

    pipeline.advance_step("resolve", TaskStatus::Succeeded, "version resolved");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "resolve", TaskStatus::Succeeded, "version resolved");
    }
    result.logs.push_back("resolved version: " + selected->versionId);

    if (selected->fileUrls.empty()) {
        result.message = "selected version has no downloadable file";
        fail_result(&result, &pipeline, queue, "download", result.message);
        return result;
    }

    const auto finalPath = final_path_for(*instance, request, *selected);
    const auto stagingPath = staging_path_for(*instance, request, *selected);
    auto rollback_artifacts = [&](const std::string& step, bool includeFinalPath, bool includeLockPath) {
        cleanup_target(&result, step, "remove staging", stagingPath);
        if (includeFinalPath) {
            cleanup_target(&result, step, "remove deployed artifact", finalPath);
        }
        if (includeLockPath && !result.lockPath.empty()) {
            cleanup_target(&result, step, "remove lock", result.lockPath);
        }
    };

    DownloadRequest downloadRequest;
    downloadRequest.id = make_install_id(request);
    downloadRequest.title = request.projectId + "@" + selected->versionId;
    downloadRequest.url = selected->fileUrls.front();
    downloadRequest.destination = stagingPath;
    downloadRequest.retryCount = 1;
    downloadRequest.overwriteExisting = true;

    result.downloadResult = downloadService_.execute(downloadRequest);
    add_logs(&result, result.downloadResult.logs, "download: ");
    if (!result.downloadResult.success) {
        result.message = result.downloadResult.error.empty() ? "download failed" : result.downloadResult.error;
        fail_result(&result, &pipeline, queue, "download", result.message);
        rollback_artifacts("download", false, false);
        return result;
    }

    pipeline.advance_step("download", TaskStatus::Succeeded, "artifact downloaded");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "download", TaskStatus::Succeeded, "artifact downloaded");
    }

    std::string deployError;
    if (!ensure_directory(finalPath.parent_path(), &deployError)) {
        result.message = deployError;
        fail_result(&result, &pipeline, queue, "deploy", result.message);
        return result;
    }

    std::error_code ec;
    std::filesystem::copy_file(stagingPath, finalPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        result.message = ec.message();
        fail_result(&result, &pipeline, queue, "deploy", result.message);
        rollback_artifacts("deploy", false, false);
        return result;
    }
    result.deployedPath = finalPath;
    pipeline.advance_step("deploy", TaskStatus::Succeeded, "artifact deployed");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "deploy", TaskStatus::Succeeded, "artifact deployed");
    }
    result.logs.push_back("deployed to " + finalPath.generic_string());

    result.lock.provider = provider_name(request);
    result.lock.projectId = request.projectId;
    result.lock.versionId = selected->versionId;
    result.lock.fileHash = result.downloadResult.artifact.checksum;
    result.lock.installedPath = finalPath;
    result.lock.enabled = true;
    result.lock.dependencies = merge_dependencies(*selected, dependencyCheck.graph);

    result.lockPath = lock_path_for(request, *instance);
    if (!write_text_file(result.lockPath, content_lock_to_text(result.lock), &deployError)) {
        result.message = deployError;
        fail_result(&result, &pipeline, queue, "lock", result.message);
        rollback_artifacts("lock", true, true);
        return result;
    }
    pipeline.advance_step("lock", TaskStatus::Succeeded, "content lock written");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "lock", TaskStatus::Succeeded, "content lock written");
    }

    {
        std::error_code ec;
        const auto removed = std::filesystem::remove_all(stagingPath, ec);
        if (ec) {
            result.logs.push_back("cleanup: failed to remove staging " + stagingPath.generic_string() + ": " + ec.message());
        } else if (removed > 0) {
            result.logs.push_back("cleanup: removed staging " + stagingPath.generic_string());
        }
    }
    result.plan = pipeline.plan();
    result.taskResult = pipeline.finish("content installed");
    result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
    result.status = ContentInstallStatus::Succeeded;
    result.success = true;
    result.message = "content installed";
    return result;
}

} // namespace dawn::core
