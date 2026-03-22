#include "app_view_model.h"

#include "dawn/core/service/instance_service.h"
#include "dawn/infra/fs/file_system.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <map>
#include <memory>

using namespace dawn::core;
using namespace dawn::ui;

namespace {

class PreviewContentProvider final : public IContentProvider {
public:
    SearchResult search(const SearchQuery&) override {
        SearchResult result;
        result.items = items_;
        return result;
    }

    std::vector<ContentVersion> versions(const std::string& projectId) override {
        const auto it = versionMap_.find(projectId);
        if (it != versionMap_.end()) {
            return it->second;
        }
        if (projectId == "project-one") {
            return versions_;
        }
        return {};
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
            {"download", "Download", TaskStatus::Pending, 0, {}},
            {"install", "Install", TaskStatus::Pending, 0, {}},
        };
        return plan;
    }

    std::vector<SearchResultItem> items_;
    std::vector<ContentVersion> versions_;
    std::map<std::string, std::vector<ContentVersion>> versionMap_;
    DependencyGraph dependencyGraph_;
};

InstanceManifest create_instance(const std::filesystem::path& root) {
    InstanceService service(root);
    InstanceManifest manifest;
    manifest.name = "ViewModel Instance";
    manifest.mcVersion = "1.20.1";
    manifest.loaderType = LoaderType::Fabric;
    manifest.loaderVersion = "0.15.11";
    manifest.javaProfileId = "java-17";
    manifest.memoryProfile = "4G";

    std::string error;
    EXPECT_TRUE(service.create_instance(manifest, &error)) << error;
    return manifest;
}

} // namespace

int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(AppViewModel, SearchSelectionProducesInstallPreview) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-preview";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);

    auto provider = std::make_shared<PreviewContentProvider>();
    provider->items_ = {
        {
            "project-one",
            "Project One",
            "Selected through search",
            "Dawn",
            "",
            "2026-03-22",
            1200,
            ProjectType::Mod,
            {"1.20.1"},
            {LoaderType::Fabric},
        }
    };

    ContentVersion first;
    first.versionId = "1.0.0";
    first.name = "First Build";
    first.fileUrls = {"https://example.invalid/first.jar"};
    first.loaders = {LoaderType::Fabric};
    first.gameVersions = {"1.20.1"};

    ContentVersion second;
    second.versionId = "1.1.0";
    second.name = "Recommended Build";
    second.fileUrls = {"https://example.invalid/second.jar"};
    second.loaders = {LoaderType::Fabric};
    second.gameVersions = {"1.20.1"};

    provider->versions_ = {first, second};

    AppViewModel viewModel(QString::fromStdString(root.string()), provider);

    EXPECT_TRUE(viewModel.searchContent("project", "mod"));
    ASSERT_EQ(viewModel.contentSearchResults().size(), 1);
    EXPECT_TRUE(viewModel.selectSearchResult("project-one"));
    EXPECT_TRUE(viewModel.selectTargetInstance(QString::fromStdString(instance.id)));
    EXPECT_TRUE(viewModel.selectInstallVersion("1.1.0"));
    viewModel.refreshInstallPreview();

    const auto searchResults = viewModel.contentSearchResults();
    ASSERT_FALSE(searchResults.isEmpty());
    EXPECT_TRUE(searchResults.front().toMap().value("selected").toBool());

    const auto versions = viewModel.contentVersions();
    ASSERT_GE(versions.size(), 2);
    EXPECT_TRUE(versions[1].toMap().value("selected").toBool());

    const auto preview = viewModel.installPreview();
    EXPECT_EQ(preview.value("projectId").toString(), "project-one");
    EXPECT_EQ(preview.value("versionId").toString(), "1.1.0");
    EXPECT_FALSE(preview.value("blocked").toBool());
    EXPECT_EQ(viewModel.installPreviewStatus(), QStringLiteral("Install preview ready"));

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, ExecuteRepairPlanReportsStatusAndLogs) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-repair";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);

    auto provider = std::make_shared<PreviewContentProvider>();
    provider->items_ = {
        {
            "project-one",
            "Project One",
            "Selected through search",
            "Dawn",
            "",
            "2026-03-22",
            1200,
            ProjectType::Mod,
            {"1.20.1"},
            {LoaderType::Fabric},
        }
    };

    ContentVersion mainVersion;
    mainVersion.versionId = "1.0.0";
    mainVersion.name = "First Build";
    mainVersion.fileUrls = {"https://example.invalid/first.jar"};
    mainVersion.loaders = {LoaderType::Fabric};
    mainVersion.gameVersions = {"1.20.1"};
    mainVersion.dependencies = {
        {"base-lib", "1.0.0", "", DependencyRequirement::Required, "base dependency"},
    };
    provider->versions_ = {mainVersion};
    provider->dependencyGraph_.dependencies = mainVersion.dependencies;

    ContentVersion dependencyVersion;
    dependencyVersion.versionId = "1.0.0";
    dependencyVersion.name = "Base Lib";
    dependencyVersion.fileUrls = {"https://example.invalid/base-lib.jar"};
    dependencyVersion.loaders = {LoaderType::Fabric};
    dependencyVersion.gameVersions = {"1.20.1"};
    provider->versionMap_["base-lib"] = {dependencyVersion};

    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "base-lib payload"});

    AppViewModel viewModel(QString::fromStdString(root.string()), provider, client);

    EXPECT_TRUE(viewModel.searchContent("project", "mod"));
    EXPECT_TRUE(viewModel.selectSearchResult("project-one"));
    EXPECT_TRUE(viewModel.selectTargetInstance(QString::fromStdString(instance.id)));
    EXPECT_TRUE(viewModel.selectInstallVersion("1.0.0"));
    viewModel.refreshInstallPreview();

    ASSERT_TRUE(viewModel.installPreview().value("repairPlanAvailable").toBool());
    EXPECT_TRUE(viewModel.executeRepairPlan());
    EXPECT_EQ(viewModel.repairExecutionStatus(), QStringLiteral("repair plan completed"));

    const auto logs = viewModel.repairExecutionLogs();
    ASSERT_FALSE(logs.isEmpty());
    EXPECT_TRUE(logs.front().toString().contains("repair plan started"));
    EXPECT_TRUE(std::any_of(logs.begin(), logs.end(), [](const QVariant& value) {
        return value.toString().contains("repaired dependency: base-lib");
    }));
    EXPECT_GE(viewModel.taskCount(), 1);

    const auto eventCenter = viewModel.eventCenter();
    ASSERT_FALSE(eventCenter.isEmpty());
    EXPECT_EQ(eventCenter.front().toMap().value("sourceType").toString(), QStringLiteral("repair"));

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, FirstLaunchFlowCompletesAndPersists) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-first-launch";
    std::filesystem::remove_all(root);

    AppViewModel viewModel(QString::fromStdString(root.string()));
    EXPECT_TRUE(viewModel.firstLaunchVisible());
    EXPECT_EQ(viewModel.wizardStepIndex(), 0);
    ASSERT_GE(viewModel.wizardSteps().size(), 4);

    EXPECT_TRUE(viewModel.nextWizardStep());
    EXPECT_EQ(viewModel.wizardStepIndex(), 1);
    EXPECT_TRUE(viewModel.nextWizardStep());
    EXPECT_EQ(viewModel.wizardStepIndex(), 2);
    EXPECT_TRUE(viewModel.previousWizardStep());
    EXPECT_EQ(viewModel.wizardStepIndex(), 1);
    EXPECT_TRUE(viewModel.completeFirstLaunch());
    EXPECT_FALSE(viewModel.firstLaunchVisible());
    EXPECT_TRUE(viewModel.firstLaunchCompleted());

    AppViewModel reopened(QString::fromStdString(root.string()));
    EXPECT_TRUE(reopened.firstLaunchCompleted());
    EXPECT_FALSE(reopened.firstLaunchVisible());

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, CacheCleanupSummaryIsExposed) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-cache";
    std::filesystem::remove_all(root);

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(root / "cache" / "alpha.bin", "abc", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(root / "cache" / "nested" / "beta.bin", "12345", &error)) << error;

    AppViewModel viewModel(QString::fromStdString(root.string()));
    EXPECT_TRUE(viewModel.clearCache());

    const auto summary = viewModel.cacheCleanupSummary();
    EXPECT_EQ(summary.value("statusLabel").toString(), QStringLiteral("success"));
    EXPECT_EQ(summary.value("bytesBefore").toULongLong(), 8u);
    EXPECT_EQ(summary.value("bytesAfter").toULongLong(), 0u);
    EXPECT_EQ(summary.value("bytesFreed").toULongLong(), 8u);
    EXPECT_EQ(summary.value("bytesBeforeDisplay").toString(), QStringLiteral("8 B"));
    EXPECT_EQ(summary.value("bytesAfterDisplay").toString(), QStringLiteral("0 B"));
    EXPECT_FALSE(summary.value("message").toString().isEmpty());

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, BackupScheduleSettingsPersistAcrossReload) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-backup-settings";
    std::filesystem::remove_all(root);

    AppViewModel viewModel(QString::fromStdString(root.string()));
    viewModel.setBackupStrategy(QStringLiteral("scheduled"));
    viewModel.setBackupScheduleDate(QStringLiteral("2026-03-30"));
    viewModel.setBackupScheduleTime(QStringLiteral("04:45"));

    EXPECT_EQ(viewModel.backupStrategy(), QStringLiteral("scheduled"));
    EXPECT_EQ(viewModel.backupScheduleDate(), QStringLiteral("2026-03-30"));
    EXPECT_EQ(viewModel.backupScheduleTime(), QStringLiteral("04:45"));

    AppViewModel reopened(QString::fromStdString(root.string()));
    EXPECT_EQ(reopened.backupStrategy(), QStringLiteral("scheduled"));
    EXPECT_EQ(reopened.backupScheduleDate(), QStringLiteral("2026-03-30"));
    EXPECT_EQ(reopened.backupScheduleTime(), QStringLiteral("04:45"));

    reopened.setBackupScheduleDate(QStringLiteral("invalid-date"));
    reopened.setBackupScheduleTime(QStringLiteral("99:99"));
    EXPECT_EQ(reopened.backupScheduleDate(), QStringLiteral("2026-03-30"));
    EXPECT_EQ(reopened.backupScheduleTime(), QStringLiteral("04:45"));

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, HandleDroppedFileInstallsLocalModAndExposesResult) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-drop";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);
    const auto modPath = root / "drop" / "local-mod.jar";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modPath, "fabric.mod.json", &error)) << error;

    AppViewModel viewModel(QString::fromStdString(root.string()));
    const auto result = viewModel.handleDroppedFile(
        QString::fromStdString(modPath.string()),
        QString::fromStdString(instance.id));

    EXPECT_TRUE(result.value("success").toBool());
    EXPECT_EQ(result.value("status").toString(), QStringLiteral("succeeded"));
    EXPECT_EQ(result.value("detectedType").toString(), QStringLiteral("mod"));
    EXPECT_FALSE(result.value("message").toString().isEmpty());
    EXPECT_TRUE(std::filesystem::exists(result.value("deployedPath").toString().toStdString()));

    const auto lastResult = viewModel.lastDroppedFileResult();
    EXPECT_EQ(lastResult.value("path").toString(), QString::fromStdString(modPath.string()));
    EXPECT_EQ(lastResult.value("targetInstanceId").toString(), QString::fromStdString(instance.id));
    EXPECT_EQ(lastResult.value("status").toString(), QStringLiteral("succeeded"));

    const auto eventCenter = viewModel.eventCenter();
    ASSERT_EQ(eventCenter.size(), 1);
    EXPECT_EQ(eventCenter.front().toMap().value("type").toString(), QStringLiteral("drag-install"));
    EXPECT_EQ(eventCenter.front().toMap().value("sourceType").toString(), QStringLiteral("local_drop"));
    EXPECT_EQ(eventCenter.front().toMap().value("result").toString(), QStringLiteral("succeeded"));
    EXPECT_EQ(eventCenter.front().toMap().value("targetInstanceId").toString(), QString::fromStdString(instance.id));

    EXPECT_FALSE(viewModel.executeRepairPlan(QStringLiteral("missing")));
    EXPECT_EQ(viewModel.eventCenter().size(), 2);

    viewModel.setInstallLogFilter(QStringLiteral("success"));
    EXPECT_EQ(viewModel.eventCenter().size(), 1);
    EXPECT_TRUE(viewModel.eventCenter().front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("failure"));
    EXPECT_EQ(viewModel.eventCenter().size(), 1);
    EXPECT_FALSE(viewModel.eventCenter().front().toMap().value("success").toBool());

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, HandleDroppedModpackCreatesNewInstanceAndUpdatesTargetContext) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-drop-modpack";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);
    const auto modpackPath = root / "drop" / "local-pack.mrpack";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modpackPath, "modrinth.index.json", &error)) << error;

    AppViewModel viewModel(QString::fromStdString(root.string()));
    const auto result = viewModel.handleDroppedFile(
        QString::fromStdString(modpackPath.string()),
        QString::fromStdString(instance.id));

    EXPECT_TRUE(result.value("success").toBool());
    EXPECT_EQ(result.value("status").toString(), QStringLiteral("succeeded"));
    EXPECT_TRUE(result.value("requiresNewInstance").toBool());

    const auto installedInstanceId = result.value("installedInstanceId").toString();
    EXPECT_FALSE(installedInstanceId.isEmpty());
    EXPECT_NE(installedInstanceId, QString::fromStdString(instance.id));
    EXPECT_EQ(result.value("targetInstanceId").toString(), installedInstanceId);
    EXPECT_EQ(viewModel.activeInstanceId(), installedInstanceId);
    EXPECT_TRUE(viewModel.autoCreatedInstanceNoticeVisible());
    EXPECT_EQ(viewModel.autoCreatedInstanceId(), installedInstanceId);
    EXPECT_TRUE(viewModel.autoCreatedInstanceNoticeText().contains(installedInstanceId));
    EXPECT_TRUE(viewModel.openAutoCreatedInstance());
    EXPECT_EQ(viewModel.activeInstanceId(), installedInstanceId);
    viewModel.clearAutoCreatedInstanceNotice();
    EXPECT_FALSE(viewModel.autoCreatedInstanceNoticeVisible());

    const auto eventCenter = viewModel.eventCenter();
    ASSERT_EQ(eventCenter.size(), 1);
    EXPECT_EQ(eventCenter.front().toMap().value("sourceType").toString(), QStringLiteral("local_drop"));
    EXPECT_EQ(eventCenter.front().toMap().value("targetInstanceId").toString(), installedInstanceId);

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, ActiveInstanceAssetsReflectsInstanceDirectories) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-active-assets";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);
    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(std::filesystem::path(instance.gameDir) / "mods" / "example-mod.jar", "mod", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(std::filesystem::path(instance.gameDir) / "resourcepacks" / "example-pack.zip", "pack", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(std::filesystem::path(instance.gameDir) / "shaderpacks" / "example-shader.zip", "shader", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(std::filesystem::path(instance.gameDir) / "logs" / "latest.log", "log", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::ensure_directory(std::filesystem::path(instance.gameDir) / "saves" / "world-1", &error)) << error;

    AppViewModel viewModel(QString::fromStdString(root.string()));
    viewModel.setActiveInstance(QString::fromStdString(instance.id));

    const auto assets = viewModel.activeInstanceAssets();
    EXPECT_EQ(assets.value("mods").toList().size(), 1);
    EXPECT_EQ(assets.value("resourcepacks").toList().size(), 1);
    EXPECT_EQ(assets.value("shaderpacks").toList().size(), 1);
    EXPECT_EQ(assets.value("logs").toList().size(), 1);
    EXPECT_EQ(assets.value("worlds").toList().size(), 1);

    const auto runtime = assets.value("runtime").toMap();
    EXPECT_EQ(runtime.value("instanceId").toString(), QString::fromStdString(instance.id));
    EXPECT_EQ(runtime.value("gameDir").toString(), QString::fromStdString(std::filesystem::path(instance.gameDir).generic_string()));

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, AssetCommandsToggleAndRemoveFiles) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-asset-commands";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);
    const auto modsDir = std::filesystem::path(instance.gameDir) / "mods";
    const auto worldsDir = std::filesystem::path(instance.gameDir) / "saves";
    const auto modPath = modsDir / "toggle-mod.jar";
    const auto disabledModPath = modsDir / "toggle-mod.jar.disabled";
    const auto worldPath = worldsDir / "world-toggle";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modPath, "mod-bytes", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::ensure_directory(worldPath, &error)) << error;

    AppViewModel viewModel(QString::fromStdString(root.string()));
    viewModel.setActiveInstance(QString::fromStdString(instance.id));

    EXPECT_TRUE(viewModel.toggleAssetEnabled(QStringLiteral("mods"), QString::fromStdString(modPath.string()), false));
    EXPECT_FALSE(std::filesystem::exists(modPath));
    EXPECT_TRUE(std::filesystem::exists(disabledModPath));

    auto assets = viewModel.activeInstanceAssets();
    ASSERT_EQ(assets.value("mods").toList().size(), 1);
    EXPECT_EQ(assets.value("mods").toList().front().toMap().value("status").toString(), QStringLiteral("Disabled"));

    EXPECT_TRUE(viewModel.toggleAssetEnabled(QStringLiteral("mods"), QString::fromStdString(disabledModPath.string()), true));
    EXPECT_TRUE(std::filesystem::exists(modPath));
    EXPECT_FALSE(std::filesystem::exists(disabledModPath));

    EXPECT_TRUE(viewModel.removeAsset(QString::fromStdString(modPath.string())));
    EXPECT_FALSE(std::filesystem::exists(modPath));

    EXPECT_TRUE(viewModel.removeAsset(QString::fromStdString(worldPath.string())));
    EXPECT_FALSE(std::filesystem::exists(worldPath));

    viewModel.setInstallLogSourceFilter(QStringLiteral("instance_asset"));
    const auto assetEvents = viewModel.eventCenter();
    EXPECT_GE(assetEvents.size(), 4);
    EXPECT_TRUE(std::all_of(assetEvents.begin(), assetEvents.end(), [](const QVariant& value) {
        return value.toMap().value("sourceType").toString() == QStringLiteral("instance_asset");
    }));

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, AssetBatchCommandsApplyToActiveInstance) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-asset-batch";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);
    const auto modsDir = std::filesystem::path(instance.gameDir) / "mods";
    const auto logsDir = std::filesystem::path(instance.gameDir) / "logs";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modsDir / "a.jar", "a", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modsDir / "b.jar", "b", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(logsDir / "latest.log", "log", &error)) << error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(logsDir / "debug.log", "log", &error)) << error;

    AppViewModel viewModel(QString::fromStdString(root.string()));
    viewModel.setActiveInstance(QString::fromStdString(instance.id));

    EXPECT_EQ(viewModel.setAllAssetsEnabled(QStringLiteral("mods"), false), 2);
    EXPECT_TRUE(std::filesystem::exists(modsDir / "a.jar.disabled"));
    EXPECT_TRUE(std::filesystem::exists(modsDir / "b.jar.disabled"));

    EXPECT_EQ(viewModel.removeDisabledAssets(QStringLiteral("mods")), 2);
    EXPECT_FALSE(std::filesystem::exists(modsDir / "a.jar.disabled"));
    EXPECT_FALSE(std::filesystem::exists(modsDir / "b.jar.disabled"));

    EXPECT_EQ(viewModel.removeAllAssets(QStringLiteral("logs")), 2);
    EXPECT_FALSE(std::filesystem::exists(logsDir / "latest.log"));
    EXPECT_FALSE(std::filesystem::exists(logsDir / "debug.log"));

    viewModel.setInstallLogSourceFilter(QStringLiteral("instance_asset"));
    const auto assetEvents = viewModel.eventCenter();
    EXPECT_GE(assetEvents.size(), 6);

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, RemoteModpackInstallCreatesNoticeForNewInstance) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-remote-modpack";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);

    auto provider = std::make_shared<PreviewContentProvider>();
    provider->items_ = {
        {
            "modpack-project",
            "Modpack Project",
            "Selected for remote modpack install",
            "Dawn",
            "",
            "2026-03-22",
            1200,
            ProjectType::Modpack,
            {"1.20.1"},
            {LoaderType::Fabric},
        }
    };

    ContentVersion version;
    version.versionId = "5.0.0";
    version.name = "Remote Pack";
    version.fileUrls = {"https://example.invalid/remote-pack.mrpack"};
    version.loaders = {LoaderType::Fabric};
    version.gameVersions = {"1.20.1"};
    provider->versions_ = {version};
    provider->versionMap_["modpack-project"] = {version};

    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "remote modpack payload"});

    AppViewModel viewModel(QString::fromStdString(root.string()), provider, client);

    EXPECT_TRUE(viewModel.searchContent("pack", "modpack"));
    EXPECT_TRUE(viewModel.selectTargetInstance(QString::fromStdString(instance.id)));
    EXPECT_TRUE(viewModel.selectInstallVersion("5.0.0"));
    EXPECT_TRUE(viewModel.installSelectedContent());

    EXPECT_TRUE(viewModel.autoCreatedInstanceNoticeVisible());
    const auto installedInstanceId = viewModel.autoCreatedInstanceId();
    EXPECT_FALSE(installedInstanceId.isEmpty());
    EXPECT_NE(installedInstanceId, QString::fromStdString(instance.id));
    EXPECT_TRUE(viewModel.autoCreatedInstanceNoticeText().contains(installedInstanceId));
    EXPECT_TRUE(viewModel.openAutoCreatedInstance());
    EXPECT_EQ(viewModel.activeInstanceId(), installedInstanceId);

    viewModel.clearAutoCreatedInstanceNotice();
    EXPECT_FALSE(viewModel.autoCreatedInstanceNoticeVisible());

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, InstallLogsClassifySourcesAndCombineFilters) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-log-filters";
    std::filesystem::remove_all(root);

    const auto instance = create_instance(root);

    auto provider = std::make_shared<PreviewContentProvider>();
    provider->items_ = {
        {
            "remote-project",
            "Remote Project",
            "Selected for remote install",
            "Dawn",
            "",
            "2026-03-22",
            900,
            ProjectType::Mod,
            {"1.20.1"},
            {LoaderType::Fabric},
        }
    };

    ContentVersion remoteVersion;
    remoteVersion.versionId = "2.0.0";
    remoteVersion.name = "Remote Build";
    remoteVersion.fileUrls = {"https://example.invalid/remote-build.jar"};
    remoteVersion.loaders = {LoaderType::Fabric};
    remoteVersion.gameVersions = {"1.20.1"};
    provider->versions_ = {remoteVersion};
    provider->versionMap_["remote-project"] = {remoteVersion};

    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "remote build payload"});

    AppViewModel viewModel(QString::fromStdString(root.string()), provider, client);

    EXPECT_TRUE(viewModel.searchContent("remote", "mod"));
    EXPECT_TRUE(viewModel.selectTargetInstance(QString::fromStdString(instance.id)));
    EXPECT_TRUE(viewModel.selectInstallVersion("2.0.0"));
    EXPECT_TRUE(viewModel.installSelectedContent());

    const auto modPath = root / "drop" / "local-mod.jar";
    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modPath, "fabric.mod.json", &error)) << error;
    const auto localDropResult = viewModel.handleDroppedFile(
        QString::fromStdString(modPath.string()),
        QString::fromStdString(instance.id));
    EXPECT_TRUE(localDropResult.value("success").toBool());

    EXPECT_FALSE(viewModel.executeRepairPlan(QStringLiteral("missing")));

    const auto logs = viewModel.eventCenter();
    ASSERT_EQ(logs.size(), 3);

    EXPECT_EQ(logs[0].toMap().value("sourceType").toString(), QStringLiteral("repair"));
    EXPECT_EQ(logs[1].toMap().value("sourceType").toString(), QStringLiteral("local_drop"));
    EXPECT_EQ(logs[2].toMap().value("sourceType").toString(), QStringLiteral("remote_content"));

    viewModel.setInstallLogFilter(QStringLiteral("success"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("local_drop"));
    auto filtered = viewModel.eventCenter();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("local_drop"));
    EXPECT_TRUE(filtered.front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("success"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("remote_content"));
    filtered = viewModel.eventCenter();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("remote_content"));
    EXPECT_TRUE(filtered.front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("failure"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("repair"));
    filtered = viewModel.eventCenter();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("repair"));
    EXPECT_FALSE(filtered.front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("failure"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("all"));
    filtered = viewModel.eventCenter();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("repair"));

    std::filesystem::remove_all(root);
}

TEST(AppViewModel, EventCenterQueuesFiltersAndSelectsContext) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-app-view-model-event-center";
    std::filesystem::remove_all(root);

    auto provider = std::make_shared<PreviewContentProvider>();
    provider->items_ = {
        {
            "event-project",
            "Event Project",
            "Selected for event center coverage",
            "Dawn",
            "",
            "2026-03-22",
            1500,
            ProjectType::Mod,
            {"1.20.1"},
            {LoaderType::Fabric},
        }
    };

    ContentVersion version;
    version.versionId = "3.0.0";
    version.name = "Event Build";
    version.fileUrls = {"https://example.invalid/event-build.jar"};
    version.loaders = {LoaderType::Fabric};
    version.gameVersions = {"1.20.1"};
    provider->versions_ = {version};
    provider->versionMap_["event-project"] = {version};

    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "event build payload"});

    AppViewModel viewModel(QString::fromStdString(root.string()), provider, client);

    EXPECT_TRUE(viewModel.searchContent("event", "mod"));
    EXPECT_TRUE(viewModel.selectSearchResult("event-project"));
    EXPECT_TRUE(viewModel.selectInstallVersion("3.0.0"));
    viewModel.refreshInstallPreview();

    auto events = viewModel.eventCenter();
    ASSERT_EQ(events.size(), 1);
    EXPECT_EQ(events.front().toMap().value("eventType").toString(), QStringLiteral("diagnostic"));
    EXPECT_EQ(events.front().toMap().value("pageHint").toString(), QStringLiteral("content"));

    EXPECT_TRUE(viewModel.createInstance("Event Center Instance", "1.20.1", "fabric"));
    const auto instanceId = viewModel.activeInstanceId();
    EXPECT_FALSE(instanceId.isEmpty());

    EXPECT_TRUE(viewModel.installSelectedContent());

    const auto modPath = root / "drop" / "event-mod.jar";
    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(modPath, "fabric.mod.json", &error)) << error;
    const auto dropResult = viewModel.handleDroppedFile(
        QString::fromStdString(modPath.string()),
        instanceId);
    EXPECT_TRUE(dropResult.value("success").toBool());

    EXPECT_FALSE(viewModel.executeRepairPlan(QStringLiteral("missing")));

    events = viewModel.eventCenter();
    ASSERT_EQ(events.size(), 4);

    viewModel.setEventCenterTypeFilter(QStringLiteral("download"));
    auto filtered = viewModel.eventCenter();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("eventType").toString(), QStringLiteral("download"));
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("remote_content"));

    viewModel.setEventCenterTypeFilter(QStringLiteral("repair"));
    filtered = viewModel.eventCenter();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("eventType").toString(), QStringLiteral("repair"));
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("repair"));

    viewModel.setEventCenterTypeFilter(QStringLiteral("all"));
    viewModel.setInstallLogFilter(QStringLiteral("failure"));
    filtered = viewModel.eventCenter();
    ASSERT_EQ(filtered.size(), 2);
    EXPECT_TRUE(std::all_of(filtered.begin(), filtered.end(), [](const QVariant& value) {
        return !value.toMap().value("success").toBool();
    }));

    viewModel.setInstallLogFilter(QStringLiteral("all"));
    viewModel.setEventCenterTypeFilter(QStringLiteral("download"));
    events = viewModel.eventCenter();
    ASSERT_EQ(events.size(), 1);
    const auto selectedEventId = events.front().toMap().value("eventId").toString();
    EXPECT_TRUE(viewModel.selectEvent(selectedEventId));
    EXPECT_EQ(viewModel.selectedEventId(), selectedEventId);
    EXPECT_EQ(viewModel.selectedEventContext().value("eventId").toString(), selectedEventId);
    EXPECT_EQ(viewModel.selectedEventContext().value("eventType").toString(), QStringLiteral("download"));
    EXPECT_EQ(viewModel.selectedEventContext().value("instanceId").toString(), instanceId);
    EXPECT_EQ(viewModel.selectedEventContext().value("projectId").toString(), QStringLiteral("event-project"));
    EXPECT_EQ(viewModel.selectedEventContext().value("versionId").toString(), QStringLiteral("3.0.0"));
    EXPECT_EQ(viewModel.selectedEventContext().value("pageHint").toString(), QStringLiteral("content"));
    EXPECT_EQ(viewModel.eventTargetPage(), QStringLiteral("content"));
    EXPECT_EQ(viewModel.eventTargetInstanceId(), instanceId);
    EXPECT_EQ(viewModel.eventTargetProjectId(), QStringLiteral("event-project"));
    EXPECT_EQ(viewModel.selectedContentProjectId(), QStringLiteral("event-project"));
    EXPECT_EQ(viewModel.selectedContentVersionId(), QStringLiteral("3.0.0"));
    EXPECT_EQ(viewModel.activeInstanceId(), instanceId);
    EXPECT_TRUE(viewModel.navigateToEventContext());
    EXPECT_EQ(viewModel.activeInstanceTabId(), QStringLiteral("overview"));

    viewModel.setEventCenterTypeFilter(QStringLiteral("repair"));
    events = viewModel.eventCenter();
    ASSERT_EQ(events.size(), 1);
    const auto repairEventId = events.front().toMap().value("eventId").toString();
    EXPECT_TRUE(viewModel.selectEvent(repairEventId));
    EXPECT_EQ(viewModel.eventTargetPage(), QStringLiteral("logs"));
    EXPECT_EQ(viewModel.eventTargetInstanceId(), instanceId);
    EXPECT_TRUE(viewModel.navigateToEventContext());
    EXPECT_EQ(viewModel.activeInstanceTabId(), QStringLiteral("logs"));

    std::filesystem::remove_all(root);
}
