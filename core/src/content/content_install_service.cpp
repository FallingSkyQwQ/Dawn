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
    std::vector<std::string> dependencies = version.dependencies;
    for (const auto& lock : graph.locks) {
        for (const auto& dependency : lock.dependencies) {
            append_unique(dependencies, dependency);
        }
    }
    return dependencies;
}

std::string ContentInstallService::provider_name(const InstallRequest& request) {
    return request.provider.empty() ? std::string("modrinth") : request.provider;
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

    std::string instanceError;
    auto instance = load_instance(request.instanceId, &instanceError);
    if (!instance) {
        result.message = instanceError.empty() ? "instance not found" : instanceError;
        fail_result(&result, &pipeline, queue, "resolve", result.message);
        return result;
    }

    const auto versions = provider.versions(request.projectId);
    if (versions.empty()) {
        result.message = "no versions available for project";
        fail_result(&result, &pipeline, queue, "resolve", result.message);
        return result;
    }

    auto selected = std::find_if(versions.begin(), versions.end(), [&](const ContentVersion& version) {
        return request.versionId.empty() || version.versionId == request.versionId;
    });
    if (selected == versions.end()) {
        result.message = "requested version not found";
        fail_result(&result, &pipeline, queue, "resolve", result.message);
        return result;
    }

    auto dependencyGraph = provider.resolveDependencies(request);
    result.missingDependencies = dependencyGraph.missing;
    result.conflicts = dependencyGraph.conflicts;
    for (const auto& missing : result.missingDependencies) {
        result.logs.push_back("missing dependency: " + missing);
    }
    for (const auto& conflict : result.conflicts) {
        result.logs.push_back("conflict: " + conflict);
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
    result.lock.dependencies = merge_dependencies(*selected, dependencyGraph);

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
