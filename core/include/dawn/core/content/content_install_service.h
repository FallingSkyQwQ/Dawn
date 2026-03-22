#pragma once

#include "dawn/core/download/download_service.h"
#include "dawn/core/interfaces/content_provider.h"
#include "dawn/core/model/content_types.h"
#include "dawn/core/model/task_types.h"
#include "dawn/core/storage/instance_repository.h"

#include <filesystem>
#include <optional>
#include <memory>
#include <string>
#include <vector>

namespace dawn::core {

class TaskQueue;

enum class ContentInstallStatus {
    Pending,
    Succeeded,
    Failed,
    CreateInstanceRequired,
};

struct ContentInstallResult {
    bool success = false;
    bool requiresNewInstance = false;
    ContentInstallStatus status = ContentInstallStatus::Pending;
    std::string message;
    std::string installedInstanceId;
    std::string queuedTaskId;
    std::filesystem::path deployedPath;
    std::filesystem::path lockPath;
    ContentLock lock;
    DownloadResult downloadResult;
    DependencyCheckResult dependencyCheck;
    std::vector<InstallDiagnostic> diagnostics;
    TaskPlan plan;
    TaskResult taskResult;
    std::vector<std::string> logs;
    struct RollbackEvent {
        std::string step;
        std::string action;
        std::string target;
        std::string status;
        std::string message;
    };
    std::vector<RollbackEvent> rollbackEvents;
    std::vector<std::string> missingDependencies;
    std::vector<std::string> conflicts;
};

class ContentInstallService {
public:
    ContentInstallService(std::filesystem::path root, DownloadService& downloadService);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;

    TaskPlan build_install_plan(const InstallRequest& request) const;
    DependencyCheckResult preview(const InstallRequest& request, IContentProvider& provider) const;
    ContentInstallResult install(const InstallRequest& request, IContentProvider& provider, TaskQueue* queue = nullptr) const;
    ContentInstallResult install_local_file(const std::filesystem::path& sourcePath, const std::string& instanceId, TaskQueue* queue = nullptr) const;
    ContentInstallResult execute_repair_plan(const InstallRequest& request, const DependencyCheckResult& preview, IContentProvider& provider, TaskQueue* queue = nullptr) const;

    static std::filesystem::path deployment_directory_for(ProjectType type, const InstanceManifest& instance);
    static std::filesystem::path lock_path_for(const InstallRequest& request, const InstanceManifest& instance);

private:
    [[nodiscard]] std::optional<InstanceManifest> load_instance(const std::string& instanceId, std::string* error = nullptr) const;
    [[nodiscard]] static std::string make_install_id(const InstallRequest& request);
    [[nodiscard]] static std::string make_target_filename(const ContentVersion& version, const InstallRequest& request);
    [[nodiscard]] static std::filesystem::path staging_path_for(const InstanceManifest& instance, const InstallRequest& request, const ContentVersion& version);
    [[nodiscard]] static std::filesystem::path final_path_for(const InstanceManifest& instance, const InstallRequest& request, const ContentVersion& version);
    [[nodiscard]] static std::vector<std::string> merge_dependencies(const ContentVersion& version, const DependencyGraph& graph);
    [[nodiscard]] static DependencyTreeNode build_dependency_tree(
        const InstallRequest& request,
        const ContentVersion& version,
        const DependencyGraph& graph,
        const std::vector<ContentLock>& installedLocks,
        bool selectedVersionCompatible);
    [[nodiscard]] static std::vector<VersionSuggestion> build_version_suggestions(
        const InstallRequest& request,
        const InstanceManifest& instance,
        const std::vector<ContentVersion>& versions,
        const ContentVersion& selectedVersion,
        bool selectedVersionCompatible);
    [[nodiscard]] static TaskPlan build_repair_plan(const InstallRequest& request, const DependencyCheckResult& preview);
    [[nodiscard]] static std::string provider_name(const InstallRequest& request);

    std::filesystem::path root_;
    InstanceRepository repository_;
    DownloadService& downloadService_;
};

} // namespace dawn::core
