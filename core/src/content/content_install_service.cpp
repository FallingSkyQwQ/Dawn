#include "dawn/core/content/content_install_service.h"

#include "dawn/core/pipeline/task_pipeline.h"
#include "dawn/core/local/local_package_service.h"
#include "dawn/core/service/instance_service.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/core/serialization/manifest_codec.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <filesystem>
#include <optional>
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

InstanceManifest create_modpack_instance(
    const std::filesystem::path& root,
    const std::string& nameHint,
    const std::string& mcVersionHint,
    LoaderType loaderHint,
    std::string* error) {
    InstanceManifest manifest;
    manifest.name = nameHint.empty() ? "Modpack Instance" : nameHint;
    manifest.mcVersion = mcVersionHint.empty() ? "latest" : mcVersionHint;
    manifest.loaderType = loaderHint;
    manifest.loaderVersion = loaderHint == LoaderType::None ? std::string() : "latest";
    manifest.javaProfileId = "default-java";
    manifest.memoryProfile = "4G";
    manifest.themeColor = "#66a3ff";
    manifest.notes = "auto-created for modpack install";

    InstanceService service(root);
    if (!service.create_instance(manifest, error)) {
        return {};
    }
    return manifest;
}

std::filesystem::path local_staging_path_for(const InstanceManifest& instance, const std::filesystem::path& sourcePath, LocalPackageType type) {
    const std::filesystem::path gameDir = instance.gameDir.empty()
        ? (std::filesystem::path(instance.id.empty() ? "instance" : instance.id) / "game")
        : std::filesystem::path(instance.gameDir);
    return gameDir / ".dawn" / "staging" / "local" / std::string(to_string(type)) / sanitize_component(sourcePath.stem().string()) / sourcePath.filename();
}

std::filesystem::path local_final_path_for(const InstanceManifest& instance, const std::filesystem::path& sourcePath, LocalPackageType type) {
    return ContentInstallService::deployment_directory_for(LocalPackageService::project_type_for(type), instance) / sourcePath.filename();
}

InstallRequest make_local_install_request(const std::filesystem::path& sourcePath, LocalPackageType type, const std::string& hash) {
    InstallRequest request;
    request.provider = "local";
    request.instanceId = "";
    request.projectId = sanitize_component(sourcePath.stem().string());
    request.versionId = hash.empty() ? timestamp_id() : hash;
    request.projectType = LocalPackageService::project_type_for(type);
    return request;
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
        TaskPipeline pipeline(result.plan);
        pipeline.start();
        result.status = ContentInstallStatus::Pending;
        result.requiresNewInstance = true;
        result.logs.push_back("modpack install started: " + request.projectId);

        if (queue) {
            result.queuedTaskId = queue->enqueue(result.plan);
            queue->start(result.queuedTaskId);
        }

        const auto versions = provider.versions(request.projectId);
        const auto selected = std::find_if(versions.begin(), versions.end(), [&](const ContentVersion& version) {
            return request.versionId.empty() || version.versionId == request.versionId;
        });
        if (selected == versions.end()) {
            fail_result(&result, &pipeline, queue, "download", "modpack version not found");
            add_rollback_event(&result, "download", "abort install", std::filesystem::path(request.projectId), "blocked", "modpack version not found");
            return result;
        }
        if (selected->fileUrls.empty()) {
            fail_result(&result, &pipeline, queue, "download", "modpack version has no downloadable artifacts");
            add_rollback_event(&result, "download", "abort install", std::filesystem::path(request.projectId), "blocked", "modpack version has no downloadable artifacts");
            return result;
        }

        std::string instanceError;
        auto instance = create_modpack_instance(
            root_,
            selected->name.empty() ? ("Modpack " + request.projectId) : selected->name,
            selected->gameVersions.empty() ? std::string() : selected->gameVersions.front(),
            selected->loaders.empty() ? LoaderType::None : selected->loaders.front(),
            &instanceError);
        if (instance.id.empty()) {
            fail_result(&result, &pipeline, queue, "create-instance", instanceError.empty() ? "failed to create modpack instance" : instanceError);
            add_rollback_event(&result, "create-instance", "abort install", std::filesystem::path(request.projectId), "failed", result.message);
            return result;
        }
        pipeline.advance_step("create-instance", TaskStatus::Succeeded, "created instance " + instance.id);
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "create-instance", TaskStatus::Succeeded, "created instance " + instance.id);
        }
        result.logs.push_back("created instance: " + instance.id);

        const auto stagingPath = staging_path_for(instance, request, *selected);
        const auto finalPath = final_path_for(instance, request, *selected);
        result.deployedPath = finalPath;
        InstallRequest lockRequest = request;
        lockRequest.instanceId = instance.id;
        result.lockPath = lock_path_for(lockRequest, instance);

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
        downloadRequest.title = "Modpack " + request.projectId;
        downloadRequest.url = selected->fileUrls.front();
        downloadRequest.destination = stagingPath;
        downloadRequest.retryCount = 2;
        downloadRequest.overwriteExisting = true;

        result.downloadResult = downloadService_.execute(downloadRequest);
        add_logs(&result, result.downloadResult.logs, "download: ");
        if (!result.downloadResult.success) {
            result.message = result.downloadResult.error.empty() ? "download failed" : result.downloadResult.error;
            fail_result(&result, &pipeline, queue, "download", result.message);
            rollback_artifacts("download", false, false);
            return result;
        }
        pipeline.advance_step("download", TaskStatus::Succeeded, "modpack downloaded");
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "download", TaskStatus::Succeeded, "modpack downloaded");
        }

        std::string deployError;
        if (!ensure_directory(finalPath.parent_path(), &deployError)) {
            fail_result(&result, &pipeline, queue, "deploy", deployError);
            rollback_artifacts("deploy", false, false);
            return result;
        }
        std::error_code ec;
        std::filesystem::copy_file(stagingPath, finalPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            fail_result(&result, &pipeline, queue, "deploy", ec.message());
            rollback_artifacts("deploy", false, false);
            return result;
        }
        pipeline.advance_step("deploy", TaskStatus::Succeeded, "modpack artifact deployed");
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "deploy", TaskStatus::Succeeded, "modpack artifact deployed");
        }

        result.lock.provider = provider_name(request);
        result.lock.projectId = request.projectId;
        result.lock.versionId = selected->versionId;
        result.lock.fileHash = result.downloadResult.artifact.checksum;
        result.lock.installedPath = finalPath;
        result.lock.enabled = true;
        result.lock.dependencies = merge_dependencies(*selected, DependencyGraph{});
        std::string lockError;
        if (!write_text_file(result.lockPath, content_lock_to_text(result.lock), &lockError)) {
            fail_result(&result, &pipeline, queue, "lock", lockError);
            rollback_artifacts("lock", true, true);
            return result;
        }
        pipeline.advance_step("lock", TaskStatus::Succeeded, "content lock written");
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "lock", TaskStatus::Succeeded, "content lock written");
        }

        {
            std::error_code cleanupEc;
            const auto removed = std::filesystem::remove_all(stagingPath, cleanupEc);
            if (cleanupEc) {
                result.logs.push_back("cleanup: failed to remove staging " + stagingPath.generic_string() + ": " + cleanupEc.message());
            } else if (removed > 0) {
                result.logs.push_back("cleanup: removed staging " + stagingPath.generic_string());
            }
        }

        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish("modpack installed into new instance");
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.status = ContentInstallStatus::Succeeded;
        result.success = true;
        result.message = "modpack installed into new instance";
        result.installedInstanceId = instance.id;
        result.logs.push_back("target instance: " + instance.id);
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
    result.installedInstanceId = instance->id;

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

ContentInstallResult ContentInstallService::install_local_file(const std::filesystem::path& sourcePath, const std::string& instanceId, TaskQueue* queue) const {
    ContentInstallResult result;
    LocalPackageService packageService;
    const auto analysis = packageService.analyze(sourcePath);

    const auto set_task_result = [&](TaskStatus status, const std::string& summary) {
        result.taskResult.planId = result.plan.id;
        result.taskResult.status = status;
        result.taskResult.summary = summary;
        result.taskResult.logs = result.logs;
    };

    result.logs.push_back("local install requested: " + sourcePath.generic_string());
    result.diagnostics.push_back(InstallDiagnostic{
        "local_package_detection",
        analysis.type == LocalPackageType::Unknown ? PreflightSeverity::Warning : PreflightSeverity::Info,
        analysis.type == LocalPackageType::Unknown
            ? "could not classify the dropped local file"
            : std::string("detected local ") + std::string(LocalPackageService::describe(analysis.type)),
        analysis.reasons.empty() ? std::string("drop a jar or zip with a known manifest") : analysis.reasons.front(),
        analysis.type == LocalPackageType::Unknown,
    });

    if (analysis.type == LocalPackageType::Unknown) {
        result.status = ContentInstallStatus::Failed;
        result.message = "unsupported local package";
        result.logs.push_back(result.message);
        result.plan = make_plan(make_local_install_request(sourcePath, analysis.type, ""));
        result.plan.status = TaskStatus::Failed;
        set_task_result(TaskStatus::Failed, result.message);
        return result;
    }

    InstallRequest request = make_local_install_request(sourcePath, analysis.type, "");
    request.instanceId = instanceId;
    result.plan = make_plan(request);

    if (analysis.type == LocalPackageType::Modpack) {
        result.requiresNewInstance = true;
        result.status = ContentInstallStatus::Pending;
        result.logs.push_back("local modpack install started");

        TaskPipeline pipeline(result.plan);
        pipeline.start();
        if (queue) {
            result.queuedTaskId = queue->enqueue(result.plan);
            queue->start(result.queuedTaskId);
        }

        std::string instanceError;
        auto instance = create_modpack_instance(
            root_,
            sourcePath.stem().string(),
            "latest",
            LoaderType::None,
            &instanceError);
        if (instance.id.empty()) {
            fail_result(&result, &pipeline, queue, "create-instance", instanceError.empty() ? "failed to create instance for local modpack" : instanceError);
            add_rollback_event(&result, "create-instance", "abort local install", sourcePath, "failed", result.message);
            return result;
        }
        pipeline.advance_step("create-instance", TaskStatus::Succeeded, "created instance " + instance.id);
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "create-instance", TaskStatus::Succeeded, "created instance " + instance.id);
        }
        result.logs.push_back("created instance: " + instance.id);

        const auto finalPath = local_final_path_for(instance, sourcePath, analysis.type);
        const auto stagingPath = local_staging_path_for(instance, sourcePath, analysis.type);
        result.deployedPath = finalPath;
        request.instanceId = instance.id;
        result.lockPath = lock_path_for(request, instance);

        auto rollback_artifacts = [&](const std::string& step, bool includeFinalPath, bool includeLockPath) {
            cleanup_target(&result, step, "remove staging", stagingPath);
            if (includeFinalPath) {
                cleanup_target(&result, step, "remove deployed artifact", finalPath);
            }
            if (includeLockPath && !result.lockPath.empty()) {
                cleanup_target(&result, step, "remove lock", result.lockPath);
            }
        };

        std::string ioError;
        if (!ensure_directory(stagingPath.parent_path(), &ioError)) {
            fail_result(&result, &pipeline, queue, "download", ioError);
            rollback_artifacts("download", false, false);
            return result;
        }
        std::error_code ec;
        std::filesystem::copy_file(sourcePath, stagingPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            fail_result(&result, &pipeline, queue, "download", ec.message());
            rollback_artifacts("download", false, false);
            return result;
        }
        pipeline.advance_step("download", TaskStatus::Succeeded, "local modpack staged");
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "download", TaskStatus::Succeeded, "local modpack staged");
        }

        if (!ensure_directory(finalPath.parent_path(), &ioError)) {
            fail_result(&result, &pipeline, queue, "deploy", ioError);
            rollback_artifacts("deploy", false, false);
            return result;
        }
        std::filesystem::copy_file(stagingPath, finalPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            fail_result(&result, &pipeline, queue, "deploy", ec.message());
            rollback_artifacts("deploy", false, false);
            return result;
        }
        pipeline.advance_step("deploy", TaskStatus::Succeeded, "local modpack deployed");
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "deploy", TaskStatus::Succeeded, "local modpack deployed");
        }

        std::string hashError;
        const auto checksum = dawn::infra::hash::sha256_file_hex(sourcePath, &hashError);
        result.lock.provider = "local";
        result.lock.projectId = request.projectId;
        result.lock.versionId = checksum.empty() ? timestamp_id() : checksum;
        result.lock.fileHash = checksum;
        result.lock.installedPath = finalPath;
        result.lock.enabled = true;
        result.lock.dependencies.clear();

        std::string lockError;
        if (!write_text_file(result.lockPath, content_lock_to_text(result.lock), &lockError)) {
            fail_result(&result, &pipeline, queue, "lock", lockError);
            rollback_artifacts("lock", true, true);
            return result;
        }
        pipeline.advance_step("lock", TaskStatus::Succeeded, "content lock written");
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "lock", TaskStatus::Succeeded, "content lock written");
        }

        {
            std::error_code cleanupEc;
            const auto removed = std::filesystem::remove_all(stagingPath, cleanupEc);
            if (cleanupEc) {
                result.logs.push_back("cleanup: failed to remove staging " + stagingPath.generic_string() + ": " + cleanupEc.message());
            } else if (removed > 0) {
                result.logs.push_back("cleanup: removed staging " + stagingPath.generic_string());
            }
        }

        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish("local modpack installed into new instance");
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.status = ContentInstallStatus::Succeeded;
        result.success = true;
        result.message = "local modpack installed into new instance";
        result.installedInstanceId = instance.id;
        result.logs.push_back("target instance: " + instance.id);
        return result;
    }

    if (request.instanceId.empty()) {
        result.status = ContentInstallStatus::Failed;
        result.message = "target instance is required for local installs";
        result.logs.push_back(result.message);
        result.plan.status = TaskStatus::Failed;
        set_task_result(TaskStatus::Failed, result.message);
        return result;
    }

    std::string instanceError;
    const auto instance = load_instance(request.instanceId, &instanceError);
    if (!instance) {
        const auto message = instanceError.empty() ? "instance not found" : instanceError;
        fail_result(&result, nullptr, queue, "resolve", message);
        add_rollback_event(&result, "resolve", "abort local install", sourcePath, "blocked", message);
        return result;
    }
    result.installedInstanceId = instance->id;

    std::string hashError;
    const auto checksum = dawn::infra::hash::sha256_file_hex(sourcePath, &hashError);
    if (checksum.empty()) {
        result.status = ContentInstallStatus::Failed;
        result.message = hashError.empty() ? "failed to hash local file" : hashError;
        result.logs.push_back(result.message);
        result.plan.status = TaskStatus::Failed;
        add_rollback_event(&result, "resolve", "abort local install", sourcePath, "blocked", result.message);
        set_task_result(TaskStatus::Failed, result.message);
        return result;
    }

    const auto finalPath = local_final_path_for(*instance, sourcePath, analysis.type);
    const auto stagingPath = local_staging_path_for(*instance, sourcePath, analysis.type);
    result.deployedPath = finalPath;
    result.lockPath = lock_path_for(request, *instance);

    auto rollback_artifacts = [&](const std::string& step, bool includeFinalPath, bool includeLockPath) {
        cleanup_target(&result, step, "remove staging", stagingPath);
        if (includeFinalPath) {
            cleanup_target(&result, step, "remove deployed artifact", finalPath);
        }
        if (includeLockPath && !result.lockPath.empty()) {
            cleanup_target(&result, step, "remove lock", result.lockPath);
        }
    };

    if (queue) {
        result.queuedTaskId = queue->enqueue(result.plan);
        queue->start(result.queuedTaskId);
    }
    TaskPipeline pipeline(result.plan);
    pipeline.start();

    pipeline.advance_step("resolve", TaskStatus::Succeeded, "local package recognized");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "resolve", TaskStatus::Succeeded, "local package recognized");
    }
    result.logs.push_back("detected local package: " + std::string(LocalPackageService::describe(analysis.type)));

    if (!ensure_directory(stagingPath.parent_path(), &hashError)) {
        result.message = hashError;
        fail_result(&result, &pipeline, queue, "download", result.message);
        add_rollback_event(&result, "download", "abort local install", stagingPath, "failed", result.message);
        return result;
    }
    if (!ensure_directory(finalPath.parent_path(), &hashError)) {
        result.message = hashError;
        fail_result(&result, &pipeline, queue, "deploy", result.message);
        add_rollback_event(&result, "deploy", "abort local install", finalPath, "failed", result.message);
        return result;
    }

    std::error_code ec;
    std::filesystem::copy_file(sourcePath, stagingPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        result.message = ec.message();
        fail_result(&result, &pipeline, queue, "download", result.message);
        rollback_artifacts("download", false, false);
        return result;
    }

    result.downloadResult.success = true;
    result.downloadResult.state = DownloadState::Completed;
    result.downloadResult.artifact.requestId = make_install_id(request);
    result.downloadResult.artifact.title = sourcePath.filename().string();
    result.downloadResult.artifact.sourceUrl = sourcePath.generic_string();
    result.downloadResult.artifact.destination = stagingPath;
    result.downloadResult.artifact.checksum = checksum;
    result.downloadResult.artifact.bytesWritten = static_cast<std::size_t>(dawn::infra::fs::file_size(sourcePath));
    result.downloadResult.artifact.attempts = 1;
    result.downloadResult.artifact.verified = true;
    result.downloadResult.logs.push_back("staged local file to " + stagingPath.generic_string());
    add_logs(&result, result.downloadResult.logs, "download: ");

    pipeline.advance_step("download", TaskStatus::Succeeded, "local file staged");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "download", TaskStatus::Succeeded, "local file staged");
    }

    std::filesystem::copy_file(stagingPath, finalPath, std::filesystem::copy_options::overwrite_existing, ec);
    if (ec) {
        result.message = ec.message();
        fail_result(&result, &pipeline, queue, "deploy", result.message);
        rollback_artifacts("deploy", false, false);
        return result;
    }

    pipeline.advance_step("deploy", TaskStatus::Succeeded, "local file deployed");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "deploy", TaskStatus::Succeeded, "local file deployed");
    }
    result.logs.push_back("deployed to " + finalPath.generic_string());

    result.lock.provider = "local";
    result.lock.projectId = request.projectId;
    result.lock.versionId = checksum;
    result.lock.fileHash = checksum;
    result.lock.installedPath = finalPath;
    result.lock.enabled = true;
    result.lock.dependencies.clear();

    std::string lockError;
    if (!write_text_file(result.lockPath, content_lock_to_text(result.lock), &lockError)) {
        result.message = lockError;
        fail_result(&result, &pipeline, queue, "lock", result.message);
        rollback_artifacts("lock", true, true);
        return result;
    }

    pipeline.advance_step("lock", TaskStatus::Succeeded, "content lock written");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "lock", TaskStatus::Succeeded, "content lock written");
    }

    {
        std::error_code cleanupEc;
        const auto removed = std::filesystem::remove_all(stagingPath, cleanupEc);
        if (cleanupEc) {
            result.logs.push_back("cleanup: failed to remove staging " + stagingPath.generic_string() + ": " + cleanupEc.message());
        } else if (removed > 0) {
            result.logs.push_back("cleanup: removed staging " + stagingPath.generic_string());
        }
    }

    result.plan = pipeline.plan();
    result.taskResult = pipeline.finish("local file installed");
    result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
    result.status = ContentInstallStatus::Succeeded;
    result.success = true;
    result.message = "local file installed";
    return result;
}

ContentInstallResult ContentInstallService::execute_repair_plan(const InstallRequest& request, const DependencyCheckResult& preview, IContentProvider& provider, TaskQueue* queue) const {
    ContentInstallResult result;
    result.plan = preview.repairPlanAvailable ? preview.repairPlan : build_repair_plan(request, preview);
    result.status = ContentInstallStatus::Pending;
    result.message = "repair plan started";
    result.logs.push_back(result.message);

    TaskPipeline pipeline(result.plan);
    pipeline.start();

    if (queue) {
        result.queuedTaskId = queue->enqueue(result.plan);
        queue->start(result.queuedTaskId);
    }

    std::string instanceError;
    const auto instance = load_instance(request.instanceId, &instanceError);
    if (!instance) {
        const auto message = instanceError.empty() ? "instance not found" : instanceError;
        fail_result(&result, &pipeline, queue, "resolve", message);
        add_rollback_event(&result, "resolve", "abort repair", std::filesystem::path(request.instanceId), "blocked", message);
        return result;
    }

    if (preview.graph.missing.empty()) {
        pipeline.advance_step("resolve", TaskStatus::Succeeded, "no missing dependencies");
        pipeline.advance_step("download", TaskStatus::Succeeded, "no missing dependencies");
        pipeline.advance_step("install", TaskStatus::Succeeded, "no missing dependencies");
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "resolve", TaskStatus::Succeeded, "no missing dependencies");
            queue->complete_step(result.queuedTaskId, "download", TaskStatus::Succeeded, "no missing dependencies");
            queue->complete_step(result.queuedTaskId, "install", TaskStatus::Succeeded, "no missing dependencies");
        }
        result.plan = pipeline.plan();
        result.taskResult = pipeline.finish("repair plan completed");
        result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
        result.status = ContentInstallStatus::Succeeded;
        result.success = true;
        result.message = "repair plan completed";
        return result;
    }

    const auto find_dependency = [&](const std::string& projectId) -> const ContentDependency* {
        const auto it = std::find_if(preview.graph.dependencies.begin(), preview.graph.dependencies.end(), [&](const ContentDependency& dependency) {
            return dependency.projectId == projectId;
        });
        return it == preview.graph.dependencies.end() ? nullptr : &*it;
    };

    const auto choose_dependency_version = [&](const std::string& projectId, const ContentDependency* dependency) -> std::optional<ContentVersion> {
        const auto versions = provider.versions(projectId);
        if (versions.empty()) {
            return std::nullopt;
        }

        if (dependency && !dependency->versionId.empty()) {
            const auto byVersionId = std::find_if(versions.begin(), versions.end(), [&](const ContentVersion& version) {
                return version.versionId == dependency->versionId;
            });
            if (byVersionId != versions.end()) {
                return *byVersionId;
            }
        }

        const auto compatible = std::find_if(versions.begin(), versions.end(), [&](const ContentVersion& version) {
            return version_compatible_with_instance(version, *instance);
        });
        if (compatible != versions.end()) {
            return *compatible;
        }

        return versions.front();
    };

    const auto install_repaired_dependency = [&](const std::string& projectId, const ContentVersion& version) -> ContentInstallResult {
        InstallRequest dependencyRequest = request;
        dependencyRequest.projectId = projectId;
        dependencyRequest.versionId = version.versionId;
        dependencyRequest.projectType = ProjectType::Mod;

        ContentInstallResult dependencyResult;
        dependencyResult.plan = make_plan(dependencyRequest);
        TaskPipeline dependencyPipeline(dependencyResult.plan);
        dependencyPipeline.start();
        dependencyResult.status = ContentInstallStatus::Pending;
        dependencyResult.message = "repair dependency started";
        dependencyResult.logs.push_back(dependencyResult.message);

        if (version.fileUrls.empty()) {
            dependencyResult.message = "selected dependency version has no downloadable file";
            fail_result(&dependencyResult, &dependencyPipeline, nullptr, "download", dependencyResult.message);
            add_rollback_event(&dependencyResult, "download", "abort repair", std::filesystem::path(projectId), "blocked", dependencyResult.message);
            return dependencyResult;
        }

        const auto finalPath = final_path_for(*instance, dependencyRequest, version);
        const auto stagingPath = staging_path_for(*instance, dependencyRequest, version);
        dependencyResult.lockPath = lock_path_for(dependencyRequest, *instance);

        auto rollback_artifacts = [&](const std::string& step, bool includeFinalPath, bool includeLockPath) {
            cleanup_target(&dependencyResult, step, "remove staging", stagingPath);
            if (includeFinalPath) {
                cleanup_target(&dependencyResult, step, "remove deployed artifact", finalPath);
            }
            if (includeLockPath && !dependencyResult.lockPath.empty()) {
                cleanup_target(&dependencyResult, step, "remove lock", dependencyResult.lockPath);
            }
        };

        DownloadRequest downloadRequest;
        downloadRequest.id = make_install_id(dependencyRequest);
        downloadRequest.title = dependencyRequest.projectId + "@" + version.versionId;
        downloadRequest.url = version.fileUrls.front();
        downloadRequest.destination = stagingPath;
        downloadRequest.retryCount = 1;
        downloadRequest.overwriteExisting = true;

        dependencyResult.downloadResult = downloadService_.execute(downloadRequest);
        add_logs(&dependencyResult, dependencyResult.downloadResult.logs, "download: ");
        if (!dependencyResult.downloadResult.success) {
            dependencyResult.message = dependencyResult.downloadResult.error.empty() ? "download failed" : dependencyResult.downloadResult.error;
            fail_result(&dependencyResult, &dependencyPipeline, nullptr, "download", dependencyResult.message);
            rollback_artifacts("download", false, false);
            return dependencyResult;
        }

        dependencyPipeline.advance_step("download", TaskStatus::Succeeded, "artifact downloaded");
        dependencyResult.logs.push_back("downloaded dependency artifact: " + dependencyRequest.projectId);

        std::string deployError;
        if (!ensure_directory(finalPath.parent_path(), &deployError)) {
            dependencyResult.message = deployError;
            fail_result(&dependencyResult, &dependencyPipeline, nullptr, "deploy", dependencyResult.message);
            rollback_artifacts("deploy", false, false);
            return dependencyResult;
        }

        std::error_code ec;
        std::filesystem::copy_file(stagingPath, finalPath, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            dependencyResult.message = ec.message();
            fail_result(&dependencyResult, &dependencyPipeline, nullptr, "deploy", dependencyResult.message);
            rollback_artifacts("deploy", false, false);
            return dependencyResult;
        }
        dependencyResult.deployedPath = finalPath;
        dependencyPipeline.advance_step("deploy", TaskStatus::Succeeded, "artifact deployed");
        dependencyResult.logs.push_back("deployed to " + finalPath.generic_string());

        dependencyResult.lock.provider = provider_name(dependencyRequest);
        dependencyResult.lock.projectId = dependencyRequest.projectId;
        dependencyResult.lock.versionId = version.versionId;
        dependencyResult.lock.fileHash = dependencyResult.downloadResult.artifact.checksum;
        dependencyResult.lock.installedPath = finalPath;
        dependencyResult.lock.enabled = true;
        dependencyResult.lock.dependencies = merge_dependencies(version, DependencyGraph{});

        std::string lockError;
        if (!write_text_file(dependencyResult.lockPath, content_lock_to_text(dependencyResult.lock), &lockError)) {
            dependencyResult.message = lockError;
            fail_result(&dependencyResult, &dependencyPipeline, nullptr, "lock", dependencyResult.message);
            rollback_artifacts("lock", true, true);
            return dependencyResult;
        }
        dependencyPipeline.advance_step("lock", TaskStatus::Succeeded, "content lock written");

        {
            std::error_code cleanupEc;
            const auto removed = std::filesystem::remove_all(stagingPath, cleanupEc);
            if (cleanupEc) {
                dependencyResult.logs.push_back("cleanup: failed to remove staging " + stagingPath.generic_string() + ": " + cleanupEc.message());
            } else if (removed > 0) {
                dependencyResult.logs.push_back("cleanup: removed staging " + stagingPath.generic_string());
            }
        }

        dependencyResult.plan = dependencyPipeline.plan();
        dependencyResult.taskResult = dependencyPipeline.finish("repair dependency installed");
        dependencyResult.logs.insert(dependencyResult.logs.end(), dependencyResult.taskResult.logs.begin(), dependencyResult.taskResult.logs.end());
        dependencyResult.status = ContentInstallStatus::Succeeded;
        dependencyResult.success = true;
        dependencyResult.message = "repair dependency installed";
        return dependencyResult;
    };

    std::vector<ContentInstallResult> repairedDependencies;
    repairedDependencies.reserve(preview.graph.missing.size());

    const auto rollback_repaired_dependencies = [&]() {
        for (auto it = repairedDependencies.rbegin(); it != repairedDependencies.rend(); ++it) {
            if (!it->deployedPath.empty()) {
                cleanup_target(&result, "repair", "remove repaired artifact", it->deployedPath);
            }
            if (!it->lockPath.empty()) {
                cleanup_target(&result, "repair", "remove repaired lock", it->lockPath);
            }
        }
    };

    for (const auto& missingId : preview.graph.missing) {
        const auto* dependency = find_dependency(missingId);
        const auto chosenVersion = choose_dependency_version(missingId, dependency);
        if (!chosenVersion.has_value()) {
            const auto message = "no version available for missing dependency " + missingId;
            fail_result(&result, &pipeline, queue, "resolve", message);
            add_rollback_event(&result, "resolve", "abort repair", std::filesystem::path(missingId), "blocked", message);
            rollback_repaired_dependencies();
            return result;
        }

        pipeline.advance_step("resolve", TaskStatus::Running, "repairing " + missingId);
        if (queue && !result.queuedTaskId.empty()) {
            queue->complete_step(result.queuedTaskId, "resolve", TaskStatus::Running, "repairing " + missingId);
        }

        const auto dependencyInstall = install_repaired_dependency(missingId, *chosenVersion);
        add_logs(&result, dependencyInstall.logs, "repair[" + missingId + "]: ");
        result.rollbackEvents.insert(result.rollbackEvents.end(), dependencyInstall.rollbackEvents.begin(), dependencyInstall.rollbackEvents.end());
        if (!dependencyInstall.success) {
            const auto message = "repair failed for dependency " + missingId + ": " + dependencyInstall.message;
            fail_result(&result, &pipeline, queue, "download", message);
            add_rollback_event(&result, "repair", "abort repair", dependencyInstall.deployedPath.empty() ? std::filesystem::path(missingId) : dependencyInstall.deployedPath, "failed", dependencyInstall.message);
            rollback_repaired_dependencies();
            return result;
        }

        repairedDependencies.push_back(dependencyInstall);
        result.deployedPath = dependencyInstall.deployedPath;
        result.lockPath = dependencyInstall.lockPath;
        result.lock = dependencyInstall.lock;
        result.downloadResult = dependencyInstall.downloadResult;
        result.logs.push_back("repaired dependency: " + missingId);
    }

    pipeline.advance_step("resolve", TaskStatus::Succeeded, "all missing dependencies resolved");
    pipeline.advance_step("download", TaskStatus::Succeeded, "all missing dependencies downloaded");
    pipeline.advance_step("install", TaskStatus::Succeeded, "all missing dependencies installed");
    if (queue && !result.queuedTaskId.empty()) {
        queue->complete_step(result.queuedTaskId, "resolve", TaskStatus::Succeeded, "all missing dependencies resolved");
        queue->complete_step(result.queuedTaskId, "download", TaskStatus::Succeeded, "all missing dependencies downloaded");
        queue->complete_step(result.queuedTaskId, "install", TaskStatus::Succeeded, "all missing dependencies installed");
    }

    result.plan = pipeline.plan();
    result.taskResult = pipeline.finish("repair plan completed");
    result.logs.insert(result.logs.end(), result.taskResult.logs.begin(), result.taskResult.logs.end());
    result.status = ContentInstallStatus::Succeeded;
    result.success = true;
    result.message = "repair plan completed";
    return result;
}

} // namespace dawn::core
