#include "app_view_model.h"

#include "dawn/core/model/instance_workbench.h"
#include "dawn/core/provider/modrinth_provider.h"
#include "dawn/infra/net/http_client.h"

#include <algorithm>
#include <cstdint>
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

QString format_bytes(std::uintmax_t bytes) {
    static constexpr const char* kUnits[] = {"B", "KiB", "MiB", "GiB", "TiB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    const auto precision = unit == 0 ? 0 : 2;
    return QStringLiteral("%1 %2")
        .arg(QString::number(value, 'f', precision))
        .arg(QString::fromLatin1(kUnits[unit]));
}

QString java_strategy_text(dawn::core::JavaStrategy strategy) {
    switch (strategy) {
    case dawn::core::JavaStrategy::Auto:
        return QStringLiteral("auto");
    case dawn::core::JavaStrategy::Bundled:
        return QStringLiteral("bundled");
    case dawn::core::JavaStrategy::CustomPath:
        return QStringLiteral("custom-path");
    case dawn::core::JavaStrategy::Downloaded:
        return QStringLiteral("downloaded");
    }
    return QStringLiteral("auto");
}

} // namespace

AppViewModel::AppViewModel(QString dataRoot, QObject* parent)
    : AppViewModel(std::move(dataRoot), std::shared_ptr<dawn::core::IContentProvider>{}, std::shared_ptr<dawn::infra::net::HttpClient>{}, parent) {
}

AppViewModel::AppViewModel(QString dataRoot, std::shared_ptr<dawn::core::IContentProvider> contentProvider, QObject* parent)
    : AppViewModel(std::move(dataRoot), std::move(contentProvider), std::shared_ptr<dawn::infra::net::HttpClient>{}, parent) {
}

AppViewModel::AppViewModel(QString dataRoot, std::shared_ptr<dawn::core::IContentProvider> contentProvider, std::shared_ptr<dawn::infra::net::HttpClient> downloadClient, QObject* parent)
    : QObject(parent)
    , dataRoot_(std::move(dataRoot))
    , settingsService_(std::filesystem::path(dataRoot_.toStdString()))
    , settings_(settingsService_.load())
    , diskSpaceStatus_(dawn::core::SettingsService::check_low_disk_space(std::filesystem::path(dataRoot_.toStdString()), settings_.lowDiskThresholdGb))
    , instanceService_(std::filesystem::path(dataRoot_.toStdString()))
    , previewDownloadService_(downloadClient)
    , contentInstallService_(std::filesystem::path(dataRoot_.toStdString()), previewDownloadService_)
    , contentProvider_(contentProvider ? std::move(contentProvider) : std::make_shared<dawn::core::ModrinthProvider>()) {
    refresh();
    refreshSettingsState();
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

QVariantList AppViewModel::repairExecutionLogs() const {
    QVariantList logs;
    for (const auto& log : repairExecutionLogs_) {
        logs.push_back(to_qstring(log));
    }
    return logs;
}

QVariantList AppViewModel::wizardSteps() const {
    QVariantList steps;
    const auto total = 4;
    for (int index = 0; index < total; ++index) {
        const bool active = index == wizardStepIndex_;
        const bool completed = index < wizardStepIndex_ || settings_.firstLaunchCompleted;
        QString title;
        QString summary;
        switch (index) {
        case 0:
            title = QStringLiteral("Welcome");
            summary = QStringLiteral("Confirm the launcher context and setup flow.");
            break;
        case 1:
            title = QStringLiteral("Data Path");
            summary = QStringLiteral("Review the data root and cache location.");
            break;
        case 2:
            title = QStringLiteral("Java Strategy");
            summary = QStringLiteral("Choose how Dawn should resolve Java.");
            break;
        case 3:
            title = QStringLiteral("Finish");
            summary = QStringLiteral("Seal the initial setup and enter the shell.");
            break;
        default:
            break;
        }
        steps.push_back(QVariantMap{
            {"index", index},
            {"title", title},
            {"summary", summary},
            {"active", active},
            {"completed", completed},
        });
    }
    return steps;
}

int AppViewModel::wizardStepIndex() const {
    return wizardStepIndex_;
}

QString AppViewModel::installPreviewStatus() const {
    return installPreviewStatus_;
}

QString AppViewModel::repairExecutionStatus() const {
    return repairExecutionStatus_;
}

bool AppViewModel::firstLaunchCompleted() const {
    return settings_.firstLaunchCompleted;
}

bool AppViewModel::firstLaunchVisible() const {
    return !settings_.firstLaunchCompleted;
}

QString AppViewModel::uiMode() const {
    return to_qstring(std::string(dawn::core::to_string(settings_.uiMode)));
}

QString AppViewModel::javaStrategy() const {
    return java_strategy_text(settings_.javaStrategy);
}

QString AppViewModel::cachePath() const {
    const auto path = settings_.cachePath.empty() ? settingsService_.defaults().cachePath : settings_.cachePath;
    return to_qstring(path.generic_string());
}

int AppViewModel::lowDiskThresholdGb() const {
    return settings_.lowDiskThresholdGb;
}

QString AppViewModel::lowDiskWarning() const {
    return diskSpaceStatus_.low ? to_qstring(diskSpaceStatus_.message) : QString();
}

QVariantMap AppViewModel::diskSpaceStatus() const {
    return diskStatusToVariant(diskSpaceStatus_);
}

QVariantMap AppViewModel::cacheCleanupSummary() const {
    return cacheCleanupToVariant(cacheCleanupSummary_);
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
    repairExecutionLogs_.clear();
    installPreview_ = dawn::core::DependencyCheckResult{};
    installPreviewStatus_ = QStringLiteral("No install preview available");
    repairExecutionStatus_ = QStringLiteral("No repair run");

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

bool AppViewModel::nextWizardStep() {
    if (settings_.firstLaunchCompleted) {
        return false;
    }
    const auto totalSteps = 4;
    if (wizardStepIndex_ >= totalSteps - 1) {
        return false;
    }
    ++wizardStepIndex_;
    emit dataChanged();
    return true;
}

bool AppViewModel::previousWizardStep() {
    if (settings_.firstLaunchCompleted) {
        return false;
    }
    if (wizardStepIndex_ <= 0) {
        return false;
    }
    --wizardStepIndex_;
    emit dataChanged();
    return true;
}

bool AppViewModel::executeRepairPlan() {
    return executeRepairPlan(0);
}

bool AppViewModel::executeRepairPlan(const QString& planId) {
    if (planId.isEmpty()) {
        return executeRepairPlan();
    }
    if (installPreview_.repairPlanAvailable && to_qstring(installPreview_.repairPlan.id) == planId) {
        return executeRepairPlan();
    }
    repairExecutionStatus_ = QStringLiteral("Repair plan not found");
    repairExecutionLogs_.clear();
    emit dataChanged();
    return false;
}

bool AppViewModel::executeRepairPlan(int planIndex) {
    if (planIndex != 0) {
        repairExecutionStatus_ = QStringLiteral("Repair plan not available");
        repairExecutionLogs_.clear();
        emit dataChanged();
        return false;
    }

    const auto request = currentInstallRequest();
    if (!request.has_value()) {
        repairExecutionStatus_ = QStringLiteral("No install request selected");
        repairExecutionLogs_.clear();
        emit dataChanged();
        return false;
    }

    if (!contentProvider_) {
        contentProvider_ = std::make_shared<dawn::core::ModrinthProvider>();
    }

    const auto result = contentInstallService_.execute_repair_plan(*request, installPreview_, *contentProvider_, &taskQueue_);
    const auto logs = result.logs;
    const auto rollbackEvents = result.rollbackEvents;
    const auto diagnostics = result.diagnostics;
    repairExecutionLogs_.clear();
    for (const auto& log : logs) {
        repairExecutionLogs_.push_back(log);
    }
    if (result.success) {
        refreshInstallPreview();
        repairExecutionLogs_.clear();
        for (const auto& log : logs) {
            repairExecutionLogs_.push_back(log);
        }
    }
    rollbackEvents_ = rollbackEvents;
    installDiagnostics_ = diagnostics;
    repairExecutionStatus_ = to_qstring(result.message.empty() ? (result.success ? "Repair completed" : "Repair failed") : result.message);
    emit dataChanged();
    return result.success;
}

bool AppViewModel::completeFirstLaunch() {
    if (settings_.firstLaunchCompleted) {
        return false;
    }

    settings_.firstLaunchCompleted = true;
    wizardStepIndex_ = 0;
    persistSettings();
    return true;
}

void AppViewModel::setUiMode(const QString& mode) {
    const auto requested = to_std(mode);
    const auto newMode = requested == "advanced" ? dawn::core::UiMode::Advanced : dawn::core::UiMode::Novice;
    if (settings_.uiMode == newMode) {
        return;
    }

    settings_.uiMode = newMode;
    persistSettings();
}

void AppViewModel::setJavaStrategy(const QString& strategy) {
    const auto requested = to_std(strategy);
    const auto parsed = requested == "bundled" ? dawn::core::JavaStrategy::Bundled
                     : requested == "custom-path" ? dawn::core::JavaStrategy::CustomPath
                     : requested == "downloaded" ? dawn::core::JavaStrategy::Downloaded
                     : dawn::core::JavaStrategy::Auto;
    if (settings_.javaStrategy == parsed) {
        return;
    }

    settings_.javaStrategy = parsed;
    persistSettings();
}

void AppViewModel::setLowDiskThresholdGb(int thresholdGb) {
    const auto clamped = std::max(0, thresholdGb);
    if (settings_.lowDiskThresholdGb == clamped) {
        return;
    }

    settings_.lowDiskThresholdGb = clamped;
    persistSettings();
}

bool AppViewModel::clearCache() {
    std::string error;
    const auto cachePath = settings_.cachePath.empty() ? settingsService_.defaults().cachePath : settings_.cachePath;
    cacheCleanupSummary_ = settingsService_.clean_cache(cachePath, &error);
    if (!cacheCleanupSummary_.success && !error.empty()) {
        cacheCleanupSummary_.message = error;
    }
    refreshDiskStatus();
    emit dataChanged();
    return cacheCleanupSummary_.success;
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

void AppViewModel::refreshSettingsState() {
    if (settings_.cachePath.empty()) {
        settings_.cachePath = settingsService_.defaults().cachePath;
    }
    refreshDiskStatus();
}

void AppViewModel::persistSettings() {
    std::string error;
    if (settings_.cachePath.empty()) {
        settings_.cachePath = settingsService_.defaults().cachePath;
    }
    settingsService_.save(settings_, &error);
    refreshSettingsState();
    emit dataChanged();
}

void AppViewModel::refreshDiskStatus() {
    std::string error;
    diskSpaceStatus_ = dawn::core::SettingsService::check_low_disk_space(std::filesystem::path(dataRoot_.toStdString()), settings_.lowDiskThresholdGb, &error);
    if (!error.empty() && diskSpaceStatus_.message.empty()) {
        diskSpaceStatus_.message = error;
    }
}

std::optional<dawn::core::InstallRequest> AppViewModel::currentInstallRequest() const {
    if (selectedContentProjectId_.isEmpty() || selectedContentVersionId_.isEmpty() || activeInstanceId().isEmpty()) {
        return std::nullopt;
    }

    dawn::core::InstallRequest request;
    request.provider = "modrinth";
    request.instanceId = to_std(activeInstanceId());
    request.projectId = to_std(selectedContentProjectId_);
    request.versionId = to_std(selectedContentVersionId_);
    request.projectType = contentSearchQuery_.projectType;
    return request;
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
    refreshDiskStatus();
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

QVariantMap AppViewModel::diskStatusToVariant(const dawn::core::DiskSpaceCheckResult& result) const {
    return QVariantMap{
        {"low", result.low},
        {"path", to_qstring(result.path.generic_string())},
        {"availableBytes", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(result.availableBytes))},
        {"thresholdBytes", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(result.thresholdBytes))},
        {"availableDisplay", format_bytes(result.availableBytes)},
        {"thresholdDisplay", format_bytes(result.thresholdBytes)},
        {"statusLabel", result.low ? QStringLiteral("warning") : QStringLiteral("ok")},
        {"message", to_qstring(result.message)},
    };
}

QVariantMap AppViewModel::cacheCleanupToVariant(const dawn::core::CacheCleanupResult& result) const {
    QVariantList logs;
    for (const auto& log : result.logs) {
        logs.push_back(to_qstring(log));
    }

    return QVariantMap{
        {"success", result.success},
        {"path", to_qstring(result.cachePath.generic_string())},
        {"bytesBefore", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(result.bytesBefore))},
        {"bytesAfter", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(result.bytesAfter))},
        {"bytesFreed", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(result.bytesFreed))},
        {"bytesBeforeDisplay", format_bytes(result.bytesBefore)},
        {"bytesAfterDisplay", format_bytes(result.bytesAfter)},
        {"bytesFreedDisplay", format_bytes(result.bytesFreed)},
        {"statusLabel", result.message.empty() ? QStringLiteral("idle") : (result.success ? QStringLiteral("success") : QStringLiteral("failed"))},
        {"filesRemoved", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(result.filesRemoved))},
        {"directoriesRemoved", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(result.directoriesRemoved))},
        {"message", to_qstring(result.message)},
        {"logs", logs},
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
