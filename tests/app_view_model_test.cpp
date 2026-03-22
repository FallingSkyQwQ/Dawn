#include "app_view_model.h"

#include "dawn/core/service/instance_service.h"
#include "dawn/infra/fs/file_system.h"

#include <QCoreApplication>

#include <gtest/gtest.h>

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

    const auto installLogs = viewModel.installLogs();
    ASSERT_FALSE(installLogs.isEmpty());
    EXPECT_EQ(installLogs.front().toMap().value("sourceType").toString(), QStringLiteral("repair"));

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

    const auto installLogs = viewModel.installLogs();
    ASSERT_EQ(installLogs.size(), 1);
    EXPECT_EQ(installLogs.front().toMap().value("type").toString(), QStringLiteral("drag-install"));
    EXPECT_EQ(installLogs.front().toMap().value("sourceType").toString(), QStringLiteral("local_drop"));
    EXPECT_EQ(installLogs.front().toMap().value("result").toString(), QStringLiteral("succeeded"));
    EXPECT_EQ(installLogs.front().toMap().value("targetInstanceId").toString(), QString::fromStdString(instance.id));

    EXPECT_FALSE(viewModel.executeRepairPlan(QStringLiteral("missing")));
    EXPECT_EQ(viewModel.installLogs().size(), 2);

    viewModel.setInstallLogFilter(QStringLiteral("success"));
    EXPECT_EQ(viewModel.installLogs().size(), 1);
    EXPECT_TRUE(viewModel.installLogs().front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("failure"));
    EXPECT_EQ(viewModel.installLogs().size(), 1);
    EXPECT_FALSE(viewModel.installLogs().front().toMap().value("success").toBool());

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

    const auto logs = viewModel.installLogs();
    ASSERT_EQ(logs.size(), 3);

    EXPECT_EQ(logs[0].toMap().value("sourceType").toString(), QStringLiteral("repair"));
    EXPECT_EQ(logs[1].toMap().value("sourceType").toString(), QStringLiteral("local_drop"));
    EXPECT_EQ(logs[2].toMap().value("sourceType").toString(), QStringLiteral("remote_content"));

    viewModel.setInstallLogFilter(QStringLiteral("success"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("local_drop"));
    auto filtered = viewModel.installLogs();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("local_drop"));
    EXPECT_TRUE(filtered.front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("success"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("remote_content"));
    filtered = viewModel.installLogs();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("remote_content"));
    EXPECT_TRUE(filtered.front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("failure"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("repair"));
    filtered = viewModel.installLogs();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("repair"));
    EXPECT_FALSE(filtered.front().toMap().value("success").toBool());

    viewModel.setInstallLogFilter(QStringLiteral("failure"));
    viewModel.setInstallLogSourceFilter(QStringLiteral("all"));
    filtered = viewModel.installLogs();
    ASSERT_EQ(filtered.size(), 1);
    EXPECT_EQ(filtered.front().toMap().value("sourceType").toString(), QStringLiteral("repair"));

    std::filesystem::remove_all(root);
}
