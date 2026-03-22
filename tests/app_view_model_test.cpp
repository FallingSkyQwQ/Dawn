#include "app_view_model.h"

#include "dawn/core/service/instance_service.h"

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

    std::filesystem::remove_all(root);
}
