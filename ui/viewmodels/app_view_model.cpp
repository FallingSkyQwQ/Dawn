#include "app_view_model.h"

#include "dawn/core/model/instance_workbench.h"
#include "dawn/core/provider/modrinth_provider.h"
#include "dawn/infra/net/http_client.h"

#include <algorithm>
#include <filesystem>
#include <memory>
#include <system_error>
#include <utility>

namespace dawn::ui {

namespace {

QString to_qstring(const std::string& value) {
    return QString::fromUtf8(value.c_str());
}

std::string to_std(const QString& value) {
    return value.toStdString();
}

QString task_status_text(dawn::core::TaskStatus status) {
    return to_qstring(std::string(dawn::core::to_string(status)));
}

QString loader_text(dawn::core::LoaderType loader) {
    return to_qstring(std::string(dawn::core::to_string(loader)));
}

QString dependency_requirement_text(dawn::core::DependencyRequirement requirement) {
    switch (requirement) {
    case dawn::core::DependencyRequirement::Required:
        return QStringLiteral("required");
    case dawn::core::DependencyRequirement::Optional:
        return QStringLiteral("optional");
    case dawn::core::DependencyRequirement::Incompatible:
        return QStringLiteral("incompatible");
    case dawn::core::DependencyRequirement::Embedded:
        return QStringLiteral("embedded");
    }
    return QStringLiteral("required");
}

QVariantList workbench_tabs_to_variant(const dawn::core::InstanceWorkbenchState& workbench) {
    QVariantList tabs;
    for (const auto& tab : workbench.tabs) {
        tabs.push_back(QVariantMap{
            {"id", to_qstring(tab.id)},
            {"title", to_qstring(tab.title)},
            {"summary", to_qstring(tab.summary)},
            {"expert", tab.expert},
        });
    }
    return tabs;
}

QVariantList strings_to_variant_list(const std::vector<std::string>& values) {
    QVariantList list;
    for (const auto& value : values) {
        list.push_back(to_qstring(value));
    }
    return list;
}

QVariantList loaders_to_variant_list(const std::vector<dawn::core::LoaderType>& values) {
    QVariantList list;
    for (const auto value : values) {
        list.push_back(to_qstring(std::string(dawn::core::to_string(value))));
    }
    return list;
}

} // namespace

AppViewModel::AppViewModel(QString dataRoot, QObject* parent)
    : AppViewModel(std::move(dataRoot), std::shared_ptr<dawn::core::IContentProvider>{}, parent) {
}

AppViewModel::AppViewModel(QString dataRoot, std::shared_ptr<dawn::core::IContentProvider> contentProvider, QObject* parent)
    : QObject(parent)
    , dataRoot_(std::move(dataRoot))
    , instanceService_(std::filesystem::path(dataRoot_.toStdString()))
    , previewDownloadService_(std::make_shared<dawn::infra::net::FakeHttpClient>())
    , contentInstallService_(std::filesystem::path(dataRoot_.toStdString()), previewDownloadService_)
    , contentProvider_(contentProvider ? std::move(contentProvider) : std::make_shared<dawn::core::ModrinthProvider>()) {
    refresh();
}

QVariantList AppViewModel::instanceCards() const {
    QVariantList cards;
    for (const auto& manifest : instances_) {
        cards.push_back(instanceToVariant(manifest));
    }
    return cards;
}

QVariantList AppViewModel::taskCards() const {
    QVariantList cards;
    for (const auto& task : taskQueue_.tasks()) {
        cards.push_back(taskToVariant(task));
    }
    return cards;
}

QVariantList AppViewModel::contentSearchResults() const {
    QVariantList results;
    for (const auto& item : contentSearchResults_) {
        results.push_back(contentSearchResultToVariant(item));
    }
    return results;
}

QVariantList AppViewModel::contentVersions() const {
    QVariantList versions;
    for (const auto& version : contentVersions_) {
        versions.push_back(contentVersionToVariant(version));
    }
    return versions;
}

QVariantList AppViewModel::instanceWorkbenchTabs() const {
    if (instances_.empty()) {
        return workbench_tabs_to_variant(dawn::core::build_instance_workbench(dawn::core::InstanceManifest{}));
    }
    const auto index = activeInstanceIndex();
    if (index < instances_.size()) {
        return workbench_tabs_to_variant(dawn::core::build_instance_workbench(instances_[index]));
    }
    return workbench_tabs_to_variant(dawn::core::build_instance_workbench(instances_.front()));
}

QVariantMap AppViewModel::activeInstanceWorkbench() const {
    if (instances_.empty()) {
        return workbenchToVariant(dawn::core::build_instance_workbench(dawn::core::InstanceManifest{}));
    }
    const auto index = activeInstanceIndex();
    if (index < instances_.size()) {
        return workbenchToVariant(dawn::core::build_instance_workbench(instances_[index]));
    }
    return workbenchToVariant(dawn::core::build_instance_workbench(instances_.front()));
}

QVariantMap AppViewModel::primaryPreflight() const {
    if (instances_.empty()) {
        return QVariantMap{
            {"ready", false},
            {"issues", QVariantList{}},
            {"recommendations", QVariantList{QStringLiteral("Create an instance to run preflight checks.")}}
        };
    }
    return preflightToVariant(preflightService_.inspect(instances_.front()));
}

QVariantMap AppViewModel::installPreview() const {
    QVariantMap preview;
    preview.insert("blocked", installPreview_.blocked);
    preview.insert("dependencyTree", dependencyTreeToVariant(installPreview_.dependencyTree));
    preview.insert("versionSuggestions", [&] {
        QVariantList suggestions;
        for (const auto& suggestion : installPreview_.versionSuggestions) {
            suggestions.push_back(versionSuggestionToVariant(suggestion));
        }
        return suggestions;
    }());
    preview.insert("repairPlanAvailable", installPreview_.repairPlanAvailable);
    preview.insert("repairPlan", repairPlanToVariant(installPreview_.repairPlan, installPreview_.repairPlanAvailable));
    preview.insert("diagnostics", installDiagnostics());
    preview.insert("rollbackEvents", rollbackEvents());
    preview.insert("targetInstanceId", selectedTargetInstanceId());
    preview.insert("projectId", selectedContentProjectId());
    preview.insert("versionId", selectedContentVersionId());
    return preview;
}

QVariantList AppViewModel::installDiagnostics() const {
    QVariantList diagnostics;
    for (const auto& diagnostic : installDiagnostics_) {
        diagnostics.push_back(diagnosticToVariant(diagnostic));
    }
    return diagnostics;
}

QVariantList AppViewModel::rollbackEvents() const {
    QVariantList events;
    for (const auto& event : rollbackEvents_) {
        events.push_back(rollbackEventToVariant(event));
    }
    return events;
}

QString AppViewModel::installPreviewStatus() const {
    return installPreviewStatus_;
}

QString AppViewModel::selectedContentProjectId() const {
    return selectedContentProjectId_;
}

QString AppViewModel::selectedContentVersionId() const {
    return selectedContentVersionId_;
}

QString AppViewModel::selectedTargetInstanceId() const {
    return activeInstanceId();
}

QString AppViewModel::primaryInstanceId() const {
    return instances_.empty() ? QString() : to_qstring(instances_.front().id);
}

QString AppViewModel::activeInstanceId() const {
    if (!activeInstanceId_.isEmpty()) {
        return activeInstanceId_;
    }
    return primaryInstanceId();
}

QString AppViewModel::activeInstanceTabId() const {
    return activeInstanceTabId_;
}

int AppViewModel::instanceCount() const {
    return static_cast<int>(instances_.size());
}

int AppViewModel::taskCount() const {
    return static_cast<int>(taskQueue_.tasks().size());
}

QString AppViewModel::dataRoot() const {
    return dataRoot_;
}

bool AppViewModel::createInstance(const QString& name, const QString& mcVersion, const QString& loaderType) {
    dawn::core::InstanceManifest manifest;
    manifest.name = to_std(name);
    manifest.mcVersion = to_std(mcVersion);
    manifest.loaderType = dawn::core::loader_type_from_string(to_std(loaderType));
    manifest.loaderVersion = manifest.loaderType == dawn::core::LoaderType::None ? std::string() : "stub";
    manifest.optifineVersion = manifest.loaderType == dawn::core::LoaderType::OptiFine ? "stub" : std::string();
    manifest.memoryProfile = "2G";
    manifest.javaProfileId = "default-java";
    manifest.themeColor = "#66a3ff";

    std::string error;
    if (!instanceService_.create_instance(manifest, &error)) {
        return false;
    }

    refresh();
    setActiveInstance(to_qstring(manifest.id));
    return true;
}

bool AppViewModel::enqueueDemoTask(const QString& title) {
    dawn::core::TaskPlan plan;
    plan.title = to_std(title);
    plan.steps = {
        {"resolve", "Resolve dependencies", dawn::core::TaskStatus::Pending, 0, {}},
        {"download", "Download artifacts", dawn::core::TaskStatus::Pending, 0, {}},
        {"install", "Install into instance", dawn::core::TaskStatus::Pending, 0, {}}
    };
    taskQueue_.enqueue(std::move(plan));
    emit dataChanged();
    return true;
}

bool AppViewModel::searchContent(const QString& text, const QString& projectType) {
    contentSearchQuery_.text = to_std(text);
    contentSearchQuery_.projectType = dawn::core::project_type_from_string(to_std(projectType));

    if (!contentProvider_) {
        contentProvider_ = std::make_shared<dawn::core::ModrinthProvider>();
    }

    const auto result = contentProvider_->search(contentSearchQuery_);
    populateSearchResults(result);
    if (!contentSearchResults_.empty()) {
        selectedContentProjectId_ = to_qstring(contentSearchResults_.front().projectId);
        refreshSelectedContentVersions();
        updateSelectedContentPreview();
    } else {
        contentVersions_.clear();
        selectedContentProjectId_.clear();
        selectedContentVersionId_.clear();
        refreshInstallPreview();
    }
    emit dataChanged();
    return !contentSearchResults_.empty();
}

bool AppViewModel::selectSearchResult(const QString& projectId) {
    const auto requested = to_std(projectId);
    const auto it = std::find_if(contentSearchResults_.begin(), contentSearchResults_.end(), [&](const dawn::core::SearchResultItem& item) {
        return item.projectId == requested;
    });
    if (it == contentSearchResults_.end()) {
        return false;
    }
    selectedContentProjectId_ = projectId;
    selectedContentVersionId_.clear();
    refreshSelectedContentVersions();
    updateSelectedContentPreview();
    emit dataChanged();
    return true;
}

bool AppViewModel::selectTargetInstance(const QString& instanceId) {
    const auto requested = to_std(instanceId);
    const auto it = std::find_if(instances_.begin(), instances_.end(), [&](const dawn::core::InstanceManifest& manifest) {
        return manifest.id == requested;
    });
    if (it == instances_.end()) {
        return false;
    }
    setActiveInstance(instanceId);
    refreshInstallPreview();
    return true;
}

bool AppViewModel::selectInstallVersion(const QString& versionId) {
    const auto requested = to_std(versionId);
    const auto it = std::find_if(contentVersions_.begin(), contentVersions_.end(), [&](const dawn::core::ContentVersion& version) {
        return version.versionId == requested;
    });
    if (it == contentVersions_.end()) {
        return false;
    }
    selectedContentVersionId_ = versionId;
    updateSelectedContentPreview();
    emit dataChanged();
    return true;
}

void AppViewModel::refreshInstallPreview() {
    installDiagnostics_.clear();
    rollbackEvents_.clear();
    installPreview_ = dawn::core::DependencyCheckResult{};
    installPreviewStatus_ = QStringLiteral("No install preview available");

    if (selectedContentProjectId_.isEmpty()) {
        if (!contentSearchResults_.empty()) {
            selectedContentProjectId_ = to_qstring(contentSearchResults_.front().projectId);
        } else {
            installPreviewStatus_ = QStringLiteral("Search content to preview install plans");
            emit dataChanged();
            return;
        }
    }

    if (selectedContentVersionId_.isEmpty()) {
        refreshSelectedContentVersions();
    }

    if (selectedContentVersionId_.isEmpty()) {
        installPreviewStatus_ = QStringLiteral("Select a version to preview installation");
        emit dataChanged();
        return;
    }

    dawn::core::InstallRequest request;
    request.provider = "modrinth";
    request.instanceId = to_std(selectedTargetInstanceId());
    request.projectId = to_std(selectedContentProjectId_);
    request.versionId = to_std(selectedContentVersionId_);
    request.projectType = contentSearchQuery_.projectType;

    if (!contentProvider_) {
        contentProvider_ = std::make_shared<dawn::core::ModrinthProvider>();
    }

    installPreview_ = contentInstallService_.preview(request, *contentProvider_);
    installDiagnostics_ = installPreview_.diagnostics;
    if (installPreview_.repairPlanAvailable) {
        installPreviewStatus_ = QStringLiteral("Repair plan available");
    } else if (installPreview_.blocked) {
        installPreviewStatus_ = QStringLiteral("Install preview blocked");
    } else {
        installPreviewStatus_ = QStringLiteral("Install preview ready");
    }
    emit dataChanged();
}

void AppViewModel::updateSelectedContentPreview() {
    refreshInstallPreview();
}

void AppViewModel::refreshSelectedContentVersions() {
    contentVersions_.clear();
    if (!contentProvider_ || selectedContentProjectId_.isEmpty()) {
        selectedContentVersionId_.clear();
        return;
    }

    contentVersions_ = contentProvider_->versions(to_std(selectedContentProjectId_));
    if (contentVersions_.empty()) {
        selectedContentVersionId_.clear();
        return;
    }

    const auto selected = std::find_if(contentVersions_.begin(), contentVersions_.end(), [&](const dawn::core::ContentVersion& version) {
        return version.versionId == to_std(selectedContentVersionId_);
    });
    if (selected == contentVersions_.end()) {
        selectedContentVersionId_ = to_qstring(contentVersions_.front().versionId);
    }
}

void AppViewModel::populateSearchResults(const dawn::core::SearchResult& result) {
    contentSearchResults_ = result.items;
}

QVariantMap AppViewModel::preflightFor(const QString& instanceId) const {
    const auto manifest = instanceService_.load_instance(to_std(instanceId));
    if (!manifest) {
        return QVariantMap{
            {"ready", false},
            {"issues", QVariantList{QVariantMap{
                {"severity", QStringLiteral("error")},
                {"code", QStringLiteral("missing_instance")},
                {"message", QStringLiteral("instance not found")},
                {"suggestion", QStringLiteral("pick an existing instance or create a new one")}
            }}},
            {"recommendations", QVariantList{}}
        };
    }
    return preflightToVariant(preflightService_.inspect(*manifest));
}

void AppViewModel::setActiveInstance(const QString& instanceId) {
    if (activeInstanceId_ == instanceId) {
        return;
    }
    const auto requestedId = to_std(instanceId);
    const auto it = std::find_if(instances_.begin(), instances_.end(), [&](const dawn::core::InstanceManifest& manifest) {
        return manifest.id == requestedId;
    });
    if (it == instances_.end()) {
        return;
    }
    activeInstanceId_ = instanceId;
    activeInstanceTabId_ = QStringLiteral("overview");
    refreshInstallPreview();
    emit dataChanged();
}

void AppViewModel::setActiveInstanceTab(const QString& tabId) {
    if (activeInstanceTabId_ == tabId) {
        return;
    }
    activeInstanceTabId_ = tabId;
    emit dataChanged();
}

void AppViewModel::refresh() {
    instances_ = instanceService_.list_instances();
    if (instances_.empty()) {
        activeInstanceId_.clear();
        activeInstanceTabId_ = QStringLiteral("overview");
    } else if (activeInstanceId_.isEmpty() || activeInstanceIndex() >= instances_.size()) {
        activeInstanceId_ = to_qstring(instances_.front().id);
        activeInstanceTabId_ = QStringLiteral("overview");
    }
    refreshInstallPreview();
    emit dataChanged();
}

QVariantMap AppViewModel::instanceToVariant(const dawn::core::InstanceManifest& manifest) const {
    const auto preflight = preflightService_.inspect(manifest);
    return QVariantMap{
        {"id", to_qstring(manifest.id)},
        {"name", to_qstring(manifest.name)},
        {"mcVersion", to_qstring(manifest.mcVersion)},
        {"loader", loader_text(manifest.loaderType)},
        {"loaderVersion", to_qstring(manifest.loaderVersion)},
        {"javaProfileId", to_qstring(manifest.javaProfileId)},
        {"resourceCount", static_cast<int>(resourceCount(manifest))},
        {"lastPlayedAt", to_qstring(manifest.lastPlayedAt)},
        {"health", preflight.ready ? QStringLiteral("Healthy") : QStringLiteral("Needs Attention")},
        {"themeColor", to_qstring(manifest.themeColor)},
        {"selected", activeInstanceId() == to_qstring(manifest.id)}
    };
}

QVariantMap AppViewModel::taskToVariant(const dawn::core::TaskPlan& plan) const {
    int doneCount = 0;
    for (const auto& step : plan.steps) {
        if (step.status == dawn::core::TaskStatus::Succeeded) {
            ++doneCount;
        }
    }
    return QVariantMap{
        {"id", to_qstring(plan.id)},
        {"title", to_qstring(plan.title)},
        {"status", task_status_text(plan.status)},
        {"stepCount", static_cast<int>(plan.steps.size())},
        {"completedSteps", doneCount}
    };
}

QVariantMap AppViewModel::stepToVariant(const dawn::core::TaskStep& step) const {
    return QVariantMap{
        {"id", to_qstring(step.id)},
        {"title", to_qstring(step.title)},
        {"status", task_status_text(step.status)},
        {"progress", step.progress},
        {"detail", to_qstring(step.detail)},
    };
}

QVariantMap AppViewModel::contentSearchResultToVariant(const dawn::core::SearchResultItem& item) const {
    return QVariantMap{
        {"projectId", to_qstring(item.projectId)},
        {"title", to_qstring(item.title)},
        {"summary", to_qstring(item.summary)},
        {"author", to_qstring(item.author)},
        {"iconUrl", to_qstring(item.iconUrl)},
        {"updatedAt", to_qstring(item.updatedAt)},
        {"downloads", static_cast<int>(item.downloads)},
        {"projectType", to_qstring(std::string(dawn::core::to_string(item.projectType)))},
        {"supportedGameVersions", strings_to_variant_list(item.supportedGameVersions)},
        {"supportedLoaders", loaders_to_variant_list(item.supportedLoaders)},
        {"selected", item.projectId == to_std(selectedContentProjectId_)},
    };
}

QVariantMap AppViewModel::contentVersionToVariant(const dawn::core::ContentVersion& version) const {
    return QVariantMap{
        {"versionId", to_qstring(version.versionId)},
        {"name", to_qstring(version.name)},
        {"fileCount", static_cast<int>(version.fileUrls.size())},
        {"gameVersions", strings_to_variant_list(version.gameVersions)},
        {"loaders", loaders_to_variant_list(version.loaders)},
        {"selected", version.versionId == to_std(selectedContentVersionId_)},
        {"recommended", false},
        {"reason", QString()},
    };
}

QVariantMap AppViewModel::versionSuggestionToVariant(const dawn::core::VersionSuggestion& suggestion) const {
    return QVariantMap{
        {"versionId", to_qstring(suggestion.versionId)},
        {"name", to_qstring(suggestion.name)},
        {"reason", to_qstring(suggestion.reason)},
        {"recommended", suggestion.recommended},
        {"gameVersions", strings_to_variant_list(suggestion.gameVersions)},
        {"loaders", loaders_to_variant_list(suggestion.loaders)},
    };
}

QVariantMap AppViewModel::dependencyTreeToVariant(const dawn::core::DependencyTreeNode& node) const {
    QVariantList children;
    for (const auto& child : node.children) {
        children.push_back(dependencyTreeToVariant(child));
    }

    return QVariantMap{
        {"id", to_qstring(node.projectId)},
        {"versionId", to_qstring(node.versionId)},
        {"requirement", dependency_requirement_text(node.requirement)},
        {"status", to_qstring(node.status)},
        {"message", to_qstring(node.message)},
        {"children", children},
    };
}

QVariantMap AppViewModel::repairPlanToVariant(const dawn::core::TaskPlan& plan, bool available) const {
    QVariantList steps;
    for (const auto& step : plan.steps) {
        steps.push_back(stepToVariant(step));
    }
    return QVariantMap{
        {"available", available},
        {"id", to_qstring(plan.id)},
        {"title", to_qstring(plan.title)},
        {"status", to_qstring(std::string(dawn::core::to_string(plan.status)))},
        {"steps", steps},
    };
}

QVariantMap AppViewModel::preflightToVariant(const dawn::core::PreflightResult& result) const {
    QVariantList issues;
    for (const auto& issue : result.issues) {
        issues.push_back(QVariantMap{
            {"severity", to_qstring(std::string(dawn::core::to_string(issue.severity)))},
            {"code", to_qstring(issue.code)},
            {"message", to_qstring(issue.message)},
            {"suggestion", to_qstring(issue.suggestion)}
        });
    }

    QVariantList recommendations;
    for (const auto& recommendation : result.recommendations) {
        recommendations.push_back(to_qstring(recommendation));
    }

    return QVariantMap{
        {"ready", result.ready},
        {"issues", issues},
        {"recommendations", recommendations}
    };
}

QVariantMap AppViewModel::workbenchToVariant(const dawn::core::InstanceWorkbenchState& workbench) const {
    return QVariantMap{
        {"instanceId", to_qstring(workbench.instanceId)},
        {"instanceName", to_qstring(workbench.instanceName)},
        {"selectedTabId", activeInstanceTabId_},
        {"tabs", workbench_tabs_to_variant(workbench)},
    };
}

QVariantMap AppViewModel::diagnosticToVariant(const dawn::core::InstallDiagnostic& diagnostic) const {
    return QVariantMap{
        {"code", to_qstring(diagnostic.code)},
        {"severity", to_qstring(std::string(dawn::core::to_string(diagnostic.severity)))},
        {"message", to_qstring(diagnostic.message)},
        {"suggestion", to_qstring(diagnostic.suggestion)},
        {"blocker", diagnostic.blocker},
    };
}

QVariantMap AppViewModel::rollbackEventToVariant(const dawn::core::ContentInstallResult::RollbackEvent& event) const {
    return QVariantMap{
        {"step", to_qstring(event.step)},
        {"action", to_qstring(event.action)},
        {"target", to_qstring(event.target)},
        {"status", to_qstring(event.status)},
        {"message", to_qstring(event.message)},
    };
}

std::size_t AppViewModel::resourceCount(const dawn::core::InstanceManifest& manifest) const {
    std::size_t count = 0;
    const std::filesystem::path gameDir = manifest.gameDir;
    const std::filesystem::path folders[] = {
        gameDir / "mods",
        gameDir / "resourcepacks",
        gameDir / "shaderpacks"
    };
    for (const auto& folder : folders) {
        std::error_code ec;
        if (!std::filesystem::exists(folder, ec)) {
            continue;
        }
        for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
            if (entry.is_regular_file()) {
                ++count;
            }
        }
    }
    return count;
}

std::size_t AppViewModel::activeInstanceIndex() const {
    const auto activeId = activeInstanceId();
    for (std::size_t index = 0; index < instances_.size(); ++index) {
        if (to_qstring(instances_[index].id) == activeId) {
            return index;
        }
    }
    return instances_.size();
}

} // namespace dawn::ui
