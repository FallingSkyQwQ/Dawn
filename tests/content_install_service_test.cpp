#include "dawn/core/content/content_install_service.h"
#include "dawn/core/serialization/manifest_codec.h"
#include "dawn/core/service/instance_service.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/net/http_client.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <memory>

using namespace dawn::core;

namespace {

class FixedContentProvider final : public IContentProvider {
public:
    SearchResult search(const SearchQuery&) override {
        return {};
    }

    std::vector<ContentVersion> versions(const std::string&) override {
        return versions_;
    }

    DependencyGraph resolveDependencies(const InstallRequest&) override {
        return dependencyGraph_;
    }

    TaskPlan buildInstallPlan(const InstallRequest& request) override {
        TaskPlan plan;
        plan.id = "provider-" + request.projectId;
        plan.title = "Provider plan";
        plan.steps = {
            {"resolve", "Resolve", TaskStatus::Pending, 0, {}},
            {"download", "Download", TaskStatus::Pending, 0, {}}
        };
        return plan;
    }

    std::vector<ContentVersion> versions_;
    DependencyGraph dependencyGraph_;
};

class ConflictPreviewProvider final : public IContentProvider {
public:
    SearchResult search(const SearchQuery&) override {
        return {};
    }

    std::vector<ContentVersion> versions(const std::string&) override {
        return versions_;
    }

    DependencyGraph resolveDependencies(const InstallRequest&) override {
        return dependencyGraph_;
    }

    TaskPlan buildInstallPlan(const InstallRequest& request) override {
        TaskPlan plan;
        plan.id = "preview-" + request.projectId;
        plan.title = "Preview";
        plan.steps = {
            {"resolve", "Resolve", TaskStatus::Pending, 0, {}},
            {"download", "Download", TaskStatus::Pending, 0, {}}
        };
        return plan;
    }

    std::vector<ContentVersion> versions_;
    DependencyGraph dependencyGraph_;
};

InstanceManifest create_instance(const std::filesystem::path& root, const std::string& name) {
    InstanceService service(root);
    InstanceManifest manifest;
    manifest.name = name;
    manifest.mcVersion = "1.20.1";
    manifest.loaderType = LoaderType::Fabric;
    manifest.loaderVersion = "0.15.11";
    manifest.javaProfileId = "java-17";
    manifest.memoryProfile = "4G";

    std::string error;
    EXPECT_TRUE(service.create_instance(manifest, &error)) << error;
    return manifest;
}

std::shared_ptr<dawn::infra::net::FakeHttpClient> make_http_client(const std::string& body) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{200, {}, body});
    return client;
}

} // namespace

TEST(ContentInstallService, InstallsModIntoModsAndWritesLock) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-content-install-mod";
    std::filesystem::remove_all(root);

    auto instance = create_instance(root, "Install Mod Instance");
    FixedContentProvider provider;
    ContentVersion version;
    version.versionId = "1.0.0";
    version.fileUrls = {"https://example.invalid/mod.jar"};
    version.dependencies = {
        {"base-lib", {}, {}, DependencyRequirement::Required, "core dependency"},
    };
    version.loaders = {LoaderType::Fabric};
    provider.versions_ = {version};
    ContentLock dependencyLock;
    dependencyLock.provider = "modrinth";
    dependencyLock.projectId = "base-lib";
    dependencyLock.versionId = "1.0.0";
    dependencyLock.fileHash = "abc";
    dependencyLock.installedPath = "mods/base-lib.jar";
    dependencyLock.enabled = true;
    dependencyLock.dependencies = {"minecraft"};
    provider.dependencyGraph_.locks.push_back(dependencyLock);

    const auto dependencyLockPath = std::filesystem::path(instance.gameDir) / "config" / "dawn" / "content-locks" / "modrinth" / "base-lib.json";
    std::string error;
    ASSERT_TRUE(save_content_lock(dependencyLockPath, dependencyLock, &error)) << error;

    DownloadService downloadService(make_http_client("mod payload"));
    ContentInstallService service(root, downloadService);
    TaskQueue queue;

    InstallRequest request;
    request.provider = "modrinth";
    request.instanceId = instance.id;
    request.projectId = "awesome-mod";
    request.versionId = "1.0.0";
    request.projectType = ProjectType::Mod;

    const auto result = service.install(request, provider, &queue);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.status, ContentInstallStatus::Succeeded);
    EXPECT_EQ(result.deployedPath.parent_path().filename().string(), "mods");
    EXPECT_TRUE(std::filesystem::exists(result.deployedPath));
    EXPECT_TRUE(std::filesystem::exists(result.lockPath));
    EXPECT_EQ(queue.tasks().size(), 1u);
    EXPECT_EQ(queue.tasks().front().status, TaskStatus::Succeeded);

    ContentLock lock;
    ASSERT_TRUE(load_content_lock(result.lockPath, &lock, &error)) << error;
    EXPECT_EQ(lock.provider, "modrinth");
    EXPECT_EQ(lock.projectId, "awesome-mod");
    EXPECT_EQ(lock.versionId, "1.0.0");
    EXPECT_EQ(lock.installedPath, result.deployedPath);
    EXPECT_FALSE(lock.fileHash.empty());
    EXPECT_NE(std::find(lock.dependencies.begin(), lock.dependencies.end(), "base-lib"), lock.dependencies.end());

    std::filesystem::remove_all(root);
}

TEST(ContentInstallService, BlocksConflictingInstallAndExposesDiagnostics) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-content-install-preview";
    std::filesystem::remove_all(root);

    auto instance = create_instance(root, "Preview Instance");
    ConflictPreviewProvider provider;
    ContentVersion version;
    version.versionId = "1.0.0";
    version.fileUrls = {"https://example.invalid/demo.jar"};
    version.loaders = {LoaderType::Forge};
    version.dependencies = {
        {"required-lib", {}, {}, DependencyRequirement::Required, "must exist"},
        {"optional-lib", {}, {}, DependencyRequirement::Optional, "nice to have"},
        {"bad-lib", {}, {}, DependencyRequirement::Incompatible, "conflict"},
    };
    provider.versions_ = {version};
    provider.dependencyGraph_.dependencies = version.dependencies;

    DownloadService downloadService(make_http_client("demo payload"));
    ContentInstallService service(root, downloadService);

    InstallRequest request;
    request.provider = "modrinth";
    request.instanceId = instance.id;
    request.projectId = "preview-mod";
    request.versionId = "1.0.0";
    request.projectType = ProjectType::Mod;

    const auto preview = service.preview(request, provider);
    EXPECT_TRUE(preview.blocked);
    EXPECT_NE(std::find_if(preview.diagnostics.begin(), preview.diagnostics.end(), [](const InstallDiagnostic& diagnostic) {
        return diagnostic.code == "loader_incompatible";
    }), preview.diagnostics.end());
    EXPECT_NE(std::find_if(preview.diagnostics.begin(), preview.diagnostics.end(), [](const InstallDiagnostic& diagnostic) {
        return diagnostic.code == "missing_required_dependency";
    }), preview.diagnostics.end());

    TaskQueue queue;
    const auto result = service.install(request, provider, &queue);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.rollbackEvents.empty());
    EXPECT_FALSE(result.diagnostics.empty());
    EXPECT_TRUE(std::any_of(result.rollbackEvents.begin(), result.rollbackEvents.end(), [](const ContentInstallResult::RollbackEvent& event) {
        return event.action == "abort install";
    }));

    std::filesystem::remove_all(root);
}

TEST(ContentInstallService, BlocksSameProjectDifferentVersionConflict) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-content-install-project-conflict";
    std::filesystem::remove_all(root);

    auto instance = create_instance(root, "Project Conflict Instance");
    FixedContentProvider provider;
    ContentVersion version;
    version.versionId = "1.0.0";
    version.fileUrls = {"https://example.invalid/demo.jar"};
    version.loaders = {LoaderType::Fabric};
    provider.versions_ = {version};

    const auto existingLockPath = std::filesystem::path(instance.gameDir) / "config" / "dawn" / "content-locks" / "legacy" / "preview-mod.json";
    std::string error;
    ContentLock existingLock;
    existingLock.provider = "legacy";
    existingLock.projectId = "preview-mod";
    existingLock.versionId = "0.9.0";
    existingLock.fileHash = "abc";
    existingLock.installedPath = "mods/preview-mod.jar";
    existingLock.enabled = true;
    ASSERT_TRUE(save_content_lock(existingLockPath, existingLock, &error)) << error;

    DownloadService downloadService(make_http_client("project payload"));
    ContentInstallService service(root, downloadService);

    InstallRequest request;
    request.provider = "modrinth";
    request.instanceId = instance.id;
    request.projectId = "preview-mod";
    request.versionId = "1.0.0";
    request.projectType = ProjectType::Mod;

    const auto preview = service.preview(request, provider);
    EXPECT_TRUE(preview.blocked);
    EXPECT_NE(std::find_if(preview.diagnostics.begin(), preview.diagnostics.end(), [](const InstallDiagnostic& diagnostic) {
        return diagnostic.code == "project_version_conflict";
    }), preview.diagnostics.end());

    const auto result = service.install(request, provider, nullptr);
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.rollbackEvents.empty());
    EXPECT_TRUE(std::any_of(result.rollbackEvents.begin(), result.rollbackEvents.end(), [](const ContentInstallResult::RollbackEvent& event) {
        return event.action == "abort install";
    }));

    std::filesystem::remove_all(root);
}

TEST(ContentInstallService, RoutesResourcepackAndShaderToTargetDirectories) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-content-install-pack";
    std::filesystem::remove_all(root);

    auto instance = create_instance(root, "Install Pack Instance");
    FixedContentProvider provider;
    ContentVersion resourcepackVersion;
    resourcepackVersion.versionId = "2.0.0";
    resourcepackVersion.fileUrls = {"https://example.invalid/resource.zip"};
    provider.versions_ = {resourcepackVersion};

    auto client = make_http_client("pack payload");
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "shader payload"});
    DownloadService downloadService(client);
    ContentInstallService service(root, downloadService);

    InstallRequest resourcepackRequest;
    resourcepackRequest.provider = "modrinth";
    resourcepackRequest.instanceId = instance.id;
    resourcepackRequest.projectId = "resourcepack";
    resourcepackRequest.versionId = "2.0.0";
    resourcepackRequest.projectType = ProjectType::Resourcepack;

    const auto resourcepackResult = service.install(resourcepackRequest, provider, nullptr);
    EXPECT_TRUE(resourcepackResult.success);
    EXPECT_EQ(resourcepackResult.deployedPath.parent_path().filename().string(), "resourcepacks");

    provider.versions_.front().versionId = "3.0.0";
    provider.versions_.front().fileUrls = {"https://example.invalid/shader.zip"};

    InstallRequest shaderRequest;
    shaderRequest.provider = "modrinth";
    shaderRequest.instanceId = instance.id;
    shaderRequest.projectId = "shaderpack";
    shaderRequest.versionId = "3.0.0";
    shaderRequest.projectType = ProjectType::Shader;

    const auto shaderResult = service.install(shaderRequest, provider, nullptr);
    EXPECT_TRUE(shaderResult.success);
    EXPECT_EQ(shaderResult.deployedPath.parent_path().filename().string(), "shaderpacks");

    std::filesystem::remove_all(root);
}

TEST(ContentInstallService, RollsBackTargetFilesWhenLockWriteFails) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-content-install-rollback";
    std::filesystem::remove_all(root);

    auto instance = create_instance(root, "Rollback Instance");
    FixedContentProvider provider;
    ContentVersion version;
    version.versionId = "1.0.0";
    version.fileUrls = {"https://example.invalid/mod.jar"};
    version.loaders = {LoaderType::Fabric};
    version.dependencies = {
        {"base-lib", {}, {}, DependencyRequirement::Required, "core dependency"},
    };
    provider.versions_ = {version};

    const auto dependencyLockPath = std::filesystem::path(instance.gameDir) / "config" / "dawn" / "content-locks" / "modrinth" / "base-lib.json";
    std::string error;
    ContentLock dependencyLock;
    dependencyLock.provider = "modrinth";
    dependencyLock.projectId = "base-lib";
    dependencyLock.versionId = "1.0.0";
    dependencyLock.fileHash = "abc";
    dependencyLock.installedPath = "mods/base-lib.jar";
    dependencyLock.enabled = true;
    dependencyLock.dependencies = {"minecraft"};
    ASSERT_TRUE(save_content_lock(dependencyLockPath, dependencyLock, &error)) << error;

    DownloadService downloadService(make_http_client("mod payload"));
    ContentInstallService service(root, downloadService);

    const auto lockParent = std::filesystem::path(instance.gameDir) / "config" / "dawn" / "content-locks" / "broken";
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(lockParent, "blocked", &error)) << error;

    InstallRequest request;
    request.provider = "broken";
    request.instanceId = instance.id;
    request.projectId = "rollback-mod";
    request.versionId = "1.0.0";
    request.projectType = ProjectType::Mod;

    const auto result = service.install(request, provider, nullptr);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.status, ContentInstallStatus::Failed);
    EXPECT_FALSE(result.downloadResult.artifact.destination.empty());
    EXPECT_FALSE(std::filesystem::exists(result.downloadResult.artifact.destination));
    EXPECT_FALSE(result.deployedPath.empty());
    EXPECT_FALSE(std::filesystem::exists(result.deployedPath));
    EXPECT_FALSE(result.lockPath.empty());
    EXPECT_FALSE(std::filesystem::exists(result.lockPath));
    ASSERT_GE(result.rollbackEvents.size(), 3u);
    EXPECT_TRUE(std::all_of(result.rollbackEvents.begin(), result.rollbackEvents.end(), [](const ContentInstallResult::RollbackEvent& event) {
        return !event.step.empty() && !event.action.empty() && !event.target.empty() && !event.status.empty();
    }));
    EXPECT_TRUE(std::any_of(result.rollbackEvents.begin(), result.rollbackEvents.end(), [](const ContentInstallResult::RollbackEvent& event) {
        return event.action == "remove staging" && event.status == "removed";
    }));
    EXPECT_TRUE(std::any_of(result.rollbackEvents.begin(), result.rollbackEvents.end(), [](const ContentInstallResult::RollbackEvent& event) {
        return event.action == "remove deployed artifact" && event.status == "removed";
    }));
    EXPECT_TRUE(std::any_of(result.rollbackEvents.begin(), result.rollbackEvents.end(), [](const ContentInstallResult::RollbackEvent& event) {
        return event.action == "remove lock" && event.status == "skipped";
    }));
    EXPECT_TRUE(std::any_of(result.logs.begin(), result.logs.end(), [](const std::string& log) {
        return log.find("rollback:") != std::string::npos;
    }));

    std::filesystem::remove_all(root);
}

TEST(ContentInstallService, ReportsModpackCreateInstanceRequired) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-content-install-modpack";
    std::filesystem::remove_all(root);

    FixedContentProvider provider;
    DownloadService downloadService(make_http_client("unused"));
    ContentInstallService service(root, downloadService);

    InstallRequest request;
    request.provider = "modrinth";
    request.instanceId = "modpack-instance";
    request.projectId = "modpack-project";
    request.versionId = "latest";
    request.projectType = ProjectType::Modpack;

    TaskQueue queue;
    const auto result = service.install(request, provider, &queue);

    EXPECT_FALSE(result.success);
    EXPECT_TRUE(result.requiresNewInstance);
    EXPECT_EQ(result.status, ContentInstallStatus::CreateInstanceRequired);
    EXPECT_EQ(result.plan.status, TaskStatus::Paused);
    EXPECT_FALSE(result.plan.steps.empty());
    EXPECT_EQ(result.plan.steps.front().id, "create-instance");
    EXPECT_EQ(queue.tasks().size(), 1u);
    EXPECT_EQ(queue.tasks().front().status, TaskStatus::Paused);

    std::filesystem::remove_all(root);
}
