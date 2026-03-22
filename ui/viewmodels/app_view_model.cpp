#include "app_view_model.h"

#include "dawn/core/model/instance_workbench.h"
#include "dawn/core/local/local_package_service.h"
#include "dawn/core/provider/modrinth_provider.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/net/http_client.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <QDate>
#include <QDateTime>
#include <QDesktopServices>
#include <QTime>
#include <QUrl>
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

QVariantList regular_files_to_variant(const std::filesystem::path& folder, bool detectDisabledSuffix) {
    QVariantList files;
    std::error_code ec;
    if (!std::filesystem::exists(folder, ec) || !std::filesystem::is_directory(folder, ec)) {
        return files;
    }

    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_regular_file()) {
            entries.push_back(entry);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return left.path().filename().generic_string() < right.path().filename().generic_string();
    });

    for (const auto& entry : entries) {
        const auto size = dawn::infra::fs::file_size(entry.path(), nullptr);
        const auto name = entry.path().filename().generic_string();
        const auto disabled = detectDisabledSuffix
            && name.size() > std::string(".disabled").size()
            && name.rfind(".disabled") == name.size() - std::string(".disabled").size();
        files.push_back(QVariantMap{
            {"name", to_qstring(name)},
            {"path", to_qstring(entry.path().generic_string())},
            {"size", QVariant::fromValue<qulonglong>(static_cast<qulonglong>(size))},
            {"sizeDisplay", format_bytes(size)},
            {"enabled", !disabled},
            {"status", disabled ? QStringLiteral("Disabled") : QStringLiteral("Enabled")},
        });
    }
    return files;
}

QVariantList directories_to_variant(const std::filesystem::path& folder) {
    QVariantList dirs;
    std::error_code ec;
    if (!std::filesystem::exists(folder, ec) || !std::filesystem::is_directory(folder, ec)) {
        return dirs;
    }

    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) {
            break;
        }
        if (entry.is_directory()) {
            entries.push_back(entry);
        }
    }
    std::sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) {
        return left.path().filename().generic_string() < right.path().filename().generic_string();
    });

    for (const auto& entry : entries) {
        dirs.push_back(QVariantMap{
            {"name", to_qstring(entry.path().filename().generic_string())},
            {"path", to_qstring(entry.path().generic_string())},
        });
    }
    return dirs;
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

QString backup_strategy_text(dawn::core::BackupStrategy strategy) {
    switch (strategy) {
    case dawn::core::BackupStrategy::Manual:
        return QStringLiteral("manual");
    case dawn::core::BackupStrategy::BeforeLaunch:
        return QStringLiteral("before-launch");
    case dawn::core::BackupStrategy::BeforeUpdate:
        return QStringLiteral("before-update");
    case dawn::core::BackupStrategy::Scheduled:
        return QStringLiteral("scheduled");
    }
    return QStringLiteral("before-update");
}

dawn::core::BackupStrategy backup_strategy_from_text(const QString& strategy) {
    const auto normalized = strategy.trimmed().toLower();
    if (normalized == QStringLiteral("manual")) {
        return dawn::core::BackupStrategy::Manual;
    }
    if (normalized == QStringLiteral("before-launch")) {
        return dawn::core::BackupStrategy::BeforeLaunch;
    }
    if (normalized == QStringLiteral("scheduled")) {
        return dawn::core::BackupStrategy::Scheduled;
    }
    return dawn::core::BackupStrategy::BeforeUpdate;
}

QString content_install_status_text(dawn::core::ContentInstallStatus status) {
    switch (status) {
    case dawn::core::ContentInstallStatus::Pending:
        return QStringLiteral("pending");
    case dawn::core::ContentInstallStatus::Succeeded:
        return QStringLiteral("succeeded");
    case dawn::core::ContentInstallStatus::Failed:
        return QStringLiteral("failed");
    case dawn::core::ContentInstallStatus::CreateInstanceRequired:
        return QStringLiteral("create-instance-required");
    }
    return QStringLiteral("pending");
}

QString current_timestamp_text() {
    return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
}

QString normalize_event_type(const QString& type, const QString& sourceType) {
    const auto normalizedType = type.trimmed().toLower();
    const auto normalizedSource = sourceType.trimmed().toLower();
    if (normalizedSource == QStringLiteral("diagnostic") || normalizedType == QStringLiteral("diagnostic")) {
        return QStringLiteral("diagnostic");
    }
    if (normalizedSource == QStringLiteral("repair") || normalizedType == QStringLiteral("repair")) {
        return QStringLiteral("repair");
    }
    if (normalizedSource == QStringLiteral("remote_content") || normalizedType.contains(QStringLiteral("download")) || normalizedType.contains(QStringLiteral("remote"))) {
        return QStringLiteral("download");
    }
    if (normalizedSource == QStringLiteral("local_drop") || normalizedType.contains(QStringLiteral("drag")) || normalizedType.contains(QStringLiteral("install"))) {
        return QStringLiteral("install");
    }
    return QStringLiteral("install");
}

QString page_hint_for_event(const QString& eventType, const QString& targetInstanceId, const QString& projectId) {
    if (!projectId.isEmpty() || eventType == QStringLiteral("download")) {
        return QStringLiteral("content");
    }
    if (!targetInstanceId.isEmpty()) {
        return QStringLiteral("instances");
    }
    if (eventType == QStringLiteral("diagnostic") || eventType == QStringLiteral("repair")) {
        return QStringLiteral("logs");
    }
    return QStringLiteral("logs");
}

int page_index_for_hint(const QString& pageHint) {
    if (pageHint == QStringLiteral("instances")) {
        return 1;
    }
    if (pageHint == QStringLiteral("content")) {
        return 2;
    }
    if (pageHint == QStringLiteral("logs")) {
        return 4;
    }
    return -1;
}

bool is_togglable_asset_type(const QString& assetType) {
    const auto type = assetType.trimmed().toLower();
    return type == QStringLiteral("mods")
        || type == QStringLiteral("resourcepacks")
        || type == QStringLiteral("shaderpacks");
}

bool is_supported_asset_type(const QString& assetType) {
    const auto type = assetType.trimmed().toLower();
    return is_togglable_asset_type(type)
        || type == QStringLiteral("logs")
        || type == QStringLiteral("worlds");
}

std::filesystem::path asset_directory_path(const std::filesystem::path& gameDir, const QString& assetType) {
    const auto type = assetType.trimmed().toLower();
    if (type == QStringLiteral("mods")) {
        return gameDir / "mods";
    }
    if (type == QStringLiteral("resourcepacks")) {
        return gameDir / "resourcepacks";
    }
    if (type == QStringLiteral("shaderpacks")) {
        return gameDir / "shaderpacks";
    }
    if (type == QStringLiteral("logs")) {
        return gameDir / "logs";
    }
    if (type == QStringLiteral("worlds")) {
        return gameDir / "saves";
    }
    return {};
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

QVariantMap AppViewModel::activeInstanceAssets() const {
    if (instances_.empty()) {
        return QVariantMap{
            {"runtime", QVariantMap{}},
            {"mods", QVariantList{}},
            {"resourcepacks", QVariantList{}},
            {"shaderpacks", QVariantList{}},
            {"worlds", QVariantList{}},
            {"logs", QVariantList{}},
        };
    }

    const auto index = activeInstanceIndex();
    const auto& manifest = index < instances_.size() ? instances_[index] : instances_.front();
    const std::filesystem::path gameDir = manifest.gameDir;
    const auto mods = regular_files_to_variant(gameDir / "mods", true);
    const auto resourcepacks = regular_files_to_variant(gameDir / "resourcepacks", true);
    const auto shaderpacks = regular_files_to_variant(gameDir / "shaderpacks", true);
    const auto worlds = directories_to_variant(gameDir / "saves");
    const auto logs = regular_files_to_variant(gameDir / "logs", false);

    return QVariantMap{
        {"runtime", QVariantMap{
            {"instanceId", to_qstring(manifest.id)},
            {"instanceName", to_qstring(manifest.name)},
            {"mcVersion", to_qstring(manifest.mcVersion)},
            {"loader", loader_text(manifest.loaderType)},
            {"loaderVersion", to_qstring(manifest.loaderVersion)},
            {"javaProfileId", to_qstring(manifest.javaProfileId)},
            {"memoryProfile", to_qstring(manifest.memoryProfile)},
            {"gameDir", to_qstring(gameDir.generic_string())},
        }},
        {"mods", mods},
        {"resourcepacks", resourcepacks},
        {"shaderpacks", shaderpacks},
        {"worlds", worlds},
        {"logs", logs},
    };
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

QVariantList AppViewModel::installLogs() const {
    QVariantList logs;
    for (auto it = installLogs_.rbegin(); it != installLogs_.rend(); ++it) {
        const auto& entry = *it;
        const auto statusFilter = installLogFilter_.toLower();
        const auto sourceFilter = installLogSourceFilter_.toLower();
        if (statusFilter == QStringLiteral("success") && !entry.success) {
            continue;
        }
        if (statusFilter == QStringLiteral("failure") && entry.success) {
            continue;
        }
        if (sourceFilter != QStringLiteral("all") && entry.sourceType.toLower() != sourceFilter) {
            continue;
        }
        logs.push_back(installLogToVariant(entry));
    }
    return logs;
}

QVariantList AppViewModel::eventCenter() const {
    QVariantList logs;
    for (auto it = installLogs_.rbegin(); it != installLogs_.rend(); ++it) {
        const auto& entry = *it;
        const auto statusFilter = installLogFilter_.toLower();
        const auto sourceFilter = installLogSourceFilter_.toLower();
        const auto typeFilter = eventCenterTypeFilter_.toLower();
        if (statusFilter == QStringLiteral("success") && !entry.success) {
            continue;
        }
        if (statusFilter == QStringLiteral("failure") && entry.success) {
            continue;
        }
        if (sourceFilter != QStringLiteral("all") && entry.sourceType.toLower() != sourceFilter) {
            continue;
        }
        if (typeFilter != QStringLiteral("all") && entry.eventType.toLower() != typeFilter) {
            continue;
        }
        logs.push_back(installLogToVariant(entry));
    }
    return logs;
}

QString AppViewModel::installLogFilter() const {
    return installLogFilter_;
}

QString AppViewModel::installLogSourceFilter() const {
    return installLogSourceFilter_;
}

QString AppViewModel::eventCenterTypeFilter() const {
    return eventCenterTypeFilter_;
}

QVariantMap AppViewModel::selectedEventContext() const {
    return selectedEventContext_;
}

QString AppViewModel::selectedEventId() const {
    return selectedEventId_;
}

QString AppViewModel::eventTargetPage() const {
    return eventTargetPage_;
}

QString AppViewModel::eventTargetInstanceId() const {
    return eventTargetInstanceId_;
}

QString AppViewModel::eventTargetProjectId() const {
    return eventTargetProjectId_;
}

QVariantList AppViewModel::repairExecutionLogs() const {
    QVariantList logs;
    for (const auto& log : repairExecutionLogs_) {
        logs.push_back(to_qstring(log));
    }
    return logs;
}

QString AppViewModel::contentInstallStatus() const {
    return contentInstallStatus_;
}

QVariantMap AppViewModel::lastDroppedFileResult() const {
    return lastDroppedFileResult_;
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

QString AppViewModel::backupStrategy() const {
    return backup_strategy_text(settings_.backupStrategy);
}

QString AppViewModel::backupScheduleDate() const {
    return to_qstring(settings_.backupScheduleDate);
}

QString AppViewModel::backupScheduleTime() const {
    return to_qstring(settings_.backupScheduleTime);
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

bool AppViewModel::autoCreatedInstanceNoticeVisible() const {
    return !autoCreatedInstanceId_.isEmpty() && !autoCreatedInstanceNoticeText_.isEmpty();
}

QString AppViewModel::autoCreatedInstanceNoticeText() const {
    return autoCreatedInstanceNoticeText_;
}

QString AppViewModel::autoCreatedInstanceId() const {
    return autoCreatedInstanceId_;
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
    manifest.loaderVersion = manifest.loaderType == dawn::core::LoaderType::None ? std::string() : "latest";
    manifest.optifineVersion = manifest.loaderType == dawn::core::LoaderType::OptiFine ? "latest" : std::string();
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
        refreshInstallPreview(false);
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
    refreshInstallPreview(false);
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

bool AppViewModel::installSelectedContent() {
    const auto request = currentInstallRequest();
    if (!request.has_value()) {
        contentInstallStatus_ = QStringLiteral("No install request selected");
        recordInstallLog(QStringLiteral("remote-install"), QStringLiteral("remote_content"), activeInstanceId(), false, contentInstallStatus_, QStringLiteral("not-configured"));
        emit dataChanged();
        return false;
    }

    if (!contentProvider_) {
        contentProvider_ = std::make_shared<dawn::core::ModrinthProvider>();
    }

    const auto result = contentInstallService_.install(*request, *contentProvider_, &taskQueue_);
    const auto effectiveInstanceId = result.installedInstanceId.empty() ? request->instanceId : result.installedInstanceId;
    contentInstallStatus_ = to_qstring(result.message.empty() ? (result.success ? "Content install completed" : "Content install failed") : result.message);
    installDiagnostics_ = result.diagnostics;
    rollbackEvents_ = result.rollbackEvents;
    repairExecutionLogs_ = result.logs;
    if (result.success) {
        refresh();
        setActiveInstance(to_qstring(effectiveInstanceId));
        if (result.requiresNewInstance || effectiveInstanceId != request->instanceId) {
            setAutoCreatedInstanceNotice(to_qstring(effectiveInstanceId), QStringLiteral("remote modpack"));
            emit navigateToPageRequested(1);
        }
        refreshInstallPreview(false);
    }

    recordInstallLog(
        QStringLiteral("remote-install"),
        QStringLiteral("remote_content"),
        to_qstring(effectiveInstanceId),
        result.success,
        contentInstallStatus_,
        result.success ? QStringLiteral("success") : QStringLiteral("failed"),
        selectedContentProjectId_,
        selectedContentVersionId_);
    emit dataChanged();
    return result.success;
}

void AppViewModel::refreshInstallPreview() {
    refreshInstallPreview(true);
}

void AppViewModel::refreshInstallPreview(bool recordDiagnosticEvent) {
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
    if (recordDiagnosticEvent && (installPreview_.blocked || !installDiagnostics_.empty())) {
        recordInstallLog(
            QStringLiteral("diagnostic"),
            QStringLiteral("diagnostic"),
            selectedTargetInstanceId(),
            !installPreview_.blocked,
            installPreviewStatus_,
            installPreview_.blocked ? QStringLiteral("blocked") : QStringLiteral("ready"),
            selectedContentProjectId_,
            selectedContentVersionId_);
    }
    emit dataChanged();
}

void AppViewModel::setInstallLogFilter(const QString& filter) {
    const auto normalized = filter.trimmed().toLower();
    const auto next = normalized.isEmpty() ? QStringLiteral("all") : normalized;
    if (installLogFilter_ == next) {
        return;
    }
    installLogFilter_ = next;
    emit dataChanged();
}

void AppViewModel::setInstallLogSourceFilter(const QString& filter) {
    const auto normalized = filter.trimmed().toLower();
    const auto next = normalized.isEmpty() ? QStringLiteral("all") : normalized;
    if (installLogSourceFilter_ == next) {
        return;
    }
    installLogSourceFilter_ = next;
    emit dataChanged();
}

void AppViewModel::setEventCenterTypeFilter(const QString& filter) {
    const auto normalized = filter.trimmed().toLower();
    const auto next = normalized.isEmpty() ? QStringLiteral("all") : normalized;
    if (eventCenterTypeFilter_ == next) {
        return;
    }
    eventCenterTypeFilter_ = next;
    emit dataChanged();
}

bool AppViewModel::selectEvent(const QString& eventId) {
    const auto requested = eventId.trimmed();
    if (requested.isEmpty()) {
        return false;
    }

    const auto it = std::find_if(installLogs_.rbegin(), installLogs_.rend(), [&](const InstallLogEntry& entry) {
        return entry.eventId == requested;
    });
    if (it == installLogs_.rend()) {
        return false;
    }

    const auto selectedIndex = static_cast<std::size_t>(std::distance(it, installLogs_.rend())) - 1;
    for (auto& entry : installLogs_) {
        entry.selected = entry.eventId == requested;
    }

    const auto& entry = installLogs_[selectedIndex];
    selectedEventId_ = entry.eventId;
    selectedEventContext_ = eventCenterContextToVariant(entry);
    eventTargetPage_ = entry.pageHint.isEmpty() ? QStringLiteral("logs") : entry.pageHint;
    eventTargetInstanceId_ = entry.targetInstanceId;
    eventTargetProjectId_ = entry.projectId;
    selectedEventContext_.insert("eventTargetPage", eventTargetPage_);
    selectedEventContext_.insert("eventTargetInstanceId", eventTargetInstanceId_);
    selectedEventContext_.insert("eventTargetProjectId", eventTargetProjectId_);

    if (!entry.targetInstanceId.isEmpty()) {
        setActiveInstance(entry.targetInstanceId);
    }
    if (!entry.projectId.isEmpty()) {
        selectedContentProjectId_ = entry.projectId;
        selectedContentVersionId_ = entry.versionId;
        refreshSelectedContentVersions();
    }
    navigateToEventContext();
    emit dataChanged();
    return true;
}

bool AppViewModel::navigateToEventContext() {
    if (eventTargetPage_.isEmpty()) {
        return false;
    }

    if (eventTargetPage_ == QStringLiteral("instances")) {
        if (!eventTargetInstanceId_.isEmpty()) {
            setActiveInstance(eventTargetInstanceId_);
        }
        const auto eventType = selectedEventContext_.value("eventType").toString().toLower();
        const auto sourceType = selectedEventContext_.value("sourceType").toString().toLower();
        if (eventType == QStringLiteral("repair") || eventType == QStringLiteral("diagnostic") || sourceType == QStringLiteral("repair")) {
            setActiveInstanceTab(QStringLiteral("logs"));
        } else {
            setActiveInstanceTab(QStringLiteral("overview"));
        }
    } else if (eventTargetPage_ == QStringLiteral("content")) {
        if (!eventTargetProjectId_.isEmpty()) {
            selectedContentProjectId_ = eventTargetProjectId_;
            refreshSelectedContentVersions();
        }
        const auto versionId = selectedEventContext_.value("versionId").toString();
        if (!versionId.isEmpty()) {
            selectedContentVersionId_ = versionId;
            refreshSelectedContentVersions();
        }
    }

    emit navigateToPageRequested(page_index_for_hint(eventTargetPage_));
    emit dataChanged();
    return true;
}

void AppViewModel::clearAutoCreatedInstanceNotice() {
    if (autoCreatedInstanceId_.isEmpty() && autoCreatedInstanceNoticeText_.isEmpty()) {
        return;
    }
    autoCreatedInstanceId_.clear();
    autoCreatedInstanceNoticeText_.clear();
    emit dataChanged();
}

bool AppViewModel::openAutoCreatedInstance() {
    if (autoCreatedInstanceId_.isEmpty()) {
        return false;
    }
    setActiveInstance(autoCreatedInstanceId_);
    setActiveInstanceTab(QStringLiteral("overview"));
    emit navigateToPageRequested(1);
    return true;
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
        recordInstallLog(QStringLiteral("repair"), QStringLiteral("repair"), selectedTargetInstanceId(), false, repairExecutionStatus_, QStringLiteral("not-found"));
    emit dataChanged();
    return false;
}

bool AppViewModel::executeRepairPlan(int planIndex) {
    if (planIndex != 0) {
        repairExecutionStatus_ = QStringLiteral("Repair plan not available");
        repairExecutionLogs_.clear();
        recordInstallLog(QStringLiteral("repair"), QStringLiteral("repair"), selectedTargetInstanceId(), false, repairExecutionStatus_, QStringLiteral("not-available"));
        emit dataChanged();
        return false;
    }

    const auto request = currentInstallRequest();
    if (!request.has_value()) {
        repairExecutionStatus_ = QStringLiteral("No install request selected");
        repairExecutionLogs_.clear();
        recordInstallLog(QStringLiteral("repair"), QStringLiteral("repair"), selectedTargetInstanceId(), false, repairExecutionStatus_, QStringLiteral("not-configured"));
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
        refreshInstallPreview(false);
        repairExecutionLogs_.clear();
        for (const auto& log : logs) {
            repairExecutionLogs_.push_back(log);
        }
    }
    rollbackEvents_ = rollbackEvents;
    installDiagnostics_ = diagnostics;
    repairExecutionStatus_ = to_qstring(result.message.empty() ? (result.success ? "Repair completed" : "Repair failed") : result.message);
    recordInstallLog(
        QStringLiteral("repair"),
        QStringLiteral("repair"),
        to_qstring(request->instanceId),
        result.success,
        repairExecutionStatus_,
        result.success ? QStringLiteral("success") : QStringLiteral("failed"),
        selectedContentProjectId_,
        selectedContentVersionId_);
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

QVariantMap AppViewModel::handleDroppedFile(const QString& path, const QString& instanceId) {
    dawn::core::LocalPackageService packageService;
    const auto sourcePath = std::filesystem::path(to_std(path));
    const auto analysis = packageService.analyze(sourcePath);
    const auto resolvedInstanceId = instanceId.isEmpty() ? activeInstanceId() : instanceId;

    QVariantMap result{
        {"path", path},
        {"targetInstanceId", resolvedInstanceId},
        {"displayName", to_qstring(analysis.displayName)},
        {"detectedType", to_qstring(std::string(dawn::core::to_string(analysis.type)))},
        {"confidence", analysis.confidence},
        {"archive", analysis.archive},
        {"reasons", strings_to_variant_list(analysis.reasons)},
        {"requiresNewInstance", analysis.type == dawn::core::LocalPackageType::Modpack},
        {"success", false},
        {"status", QStringLiteral("failed")},
    };

    if (analysis.type == dawn::core::LocalPackageType::Unknown) {
        result.insert("message", QStringLiteral("could not classify the dropped file"));
        result.insert("failureReason", QStringLiteral("ZIP structure did not match a known mod, resource pack, shader, or modpack layout"));
        result.insert("logs", QVariantList{QStringLiteral("local package analysis failed")});
        lastDroppedFileResult_ = result;
        recordInstallLog(QStringLiteral("drag-install"), QStringLiteral("local_drop"), resolvedInstanceId, false, QStringLiteral("local package analysis failed"), QStringLiteral("unknown"));
        emit dataChanged();
        return result;
    }

    const auto installResult = contentInstallService_.install_local_file(sourcePath, to_std(resolvedInstanceId), &taskQueue_);
    const auto effectiveInstanceId = installResult.installedInstanceId.empty() ? to_std(resolvedInstanceId) : installResult.installedInstanceId;
    QVariantList logs;
    for (const auto& log : installResult.logs) {
        logs.push_back(to_qstring(log));
    }
    QVariantList rollbackEvents;
    for (const auto& event : installResult.rollbackEvents) {
        rollbackEvents.push_back(rollbackEventToVariant(event));
    }
    QVariantList diagnostics;
    for (const auto& diagnostic : installResult.diagnostics) {
        diagnostics.push_back(diagnosticToVariant(diagnostic));
    }

    result.insert("success", installResult.success);
    result.insert("status", content_install_status_text(installResult.status));
    result.insert("message", to_qstring(installResult.message));
    result.insert("failureReason", installResult.success ? QString() : to_qstring(installResult.message));
    result.insert("requiresNewInstance", installResult.requiresNewInstance);
    result.insert("targetInstanceId", to_qstring(effectiveInstanceId));
    result.insert("installedInstanceId", to_qstring(effectiveInstanceId));
    result.insert("deployedPath", to_qstring(installResult.deployedPath.generic_string()));
    result.insert("lockPath", to_qstring(installResult.lockPath.generic_string()));
    result.insert("logs", logs);
    result.insert("diagnostics", diagnostics);
    result.insert("rollbackEvents", rollbackEvents);
    result.insert("planId", to_qstring(installResult.plan.id));
    result.insert("planStatus", task_status_text(installResult.plan.status));
    result.insert("taskId", to_qstring(installResult.queuedTaskId));
    result.insert("fileHash", to_qstring(installResult.lock.fileHash));
    result.insert("provider", to_qstring(installResult.lock.provider));
    result.insert("projectId", to_qstring(installResult.lock.projectId));
    result.insert("versionId", to_qstring(installResult.lock.versionId));

    recordInstallLog(
        QStringLiteral("drag-install"),
        QStringLiteral("local_drop"),
        to_qstring(effectiveInstanceId),
        installResult.success,
        installResult.message.empty() ? result.value("detectedType").toString() : to_qstring(installResult.message),
        result.value("status").toString());

    if (installResult.success && !effectiveInstanceId.empty()) {
        refresh();
        setActiveInstance(to_qstring(effectiveInstanceId));
        if (installResult.requiresNewInstance || effectiveInstanceId != to_std(resolvedInstanceId)) {
            setAutoCreatedInstanceNotice(to_qstring(effectiveInstanceId), QStringLiteral("local modpack"));
            emit navigateToPageRequested(1);
        }
    }

    lastDroppedFileResult_ = result;
    emit dataChanged();
    return result;
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

void AppViewModel::setBackupStrategy(const QString& strategy) {
    const auto parsed = backup_strategy_from_text(strategy);
    if (settings_.backupStrategy == parsed) {
        return;
    }

    settings_.backupStrategy = parsed;
    persistSettings();
}

void AppViewModel::setBackupScheduleDate(const QString& date) {
    if (date.trimmed().isEmpty()) {
        if (settings_.backupScheduleDate.empty()) {
            return;
        }
        settings_.backupScheduleDate.clear();
        persistSettings();
        return;
    }

    const auto parsed = QDate::fromString(date, Qt::ISODate);
    if (!parsed.isValid()) {
        return;
    }

    const auto normalized = parsed.toString(Qt::ISODate);
    if (to_qstring(settings_.backupScheduleDate) == normalized) {
        return;
    }

    settings_.backupScheduleDate = to_std(normalized);
    persistSettings();
}

void AppViewModel::setBackupScheduleTime(const QString& time) {
    if (time.trimmed().isEmpty()) {
        if (settings_.backupScheduleTime.empty()) {
            return;
        }
        settings_.backupScheduleTime.clear();
        persistSettings();
        return;
    }

    const auto parsed = QTime::fromString(time, QStringLiteral("HH:mm"));
    if (!parsed.isValid()) {
        return;
    }

    const auto normalized = parsed.toString(QStringLiteral("HH:mm"));
    if (to_qstring(settings_.backupScheduleTime) == normalized) {
        return;
    }

    settings_.backupScheduleTime = to_std(normalized);
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

bool AppViewModel::toggleAssetEnabled(const QString& assetType, const QString& assetPath, bool enabled) {
    const auto normalizedType = assetType.trimmed().toLower();
    if (normalizedType != QStringLiteral("mods")
        && normalizedType != QStringLiteral("resourcepacks")
        && normalizedType != QStringLiteral("shaderpacks")) {
        recordInstallLog(
            QStringLiteral("asset-toggle"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            false,
            QStringLiteral("Unsupported asset type: %1").arg(assetType),
            QStringLiteral("invalid-type"));
        return false;
    }

    if (assetPath.trimmed().isEmpty()) {
        recordInstallLog(
            QStringLiteral("asset-toggle"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            false,
            QStringLiteral("Asset path is empty"),
            QStringLiteral("invalid-path"));
        return false;
    }

    const std::filesystem::path path = to_std(assetPath);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec) || !std::filesystem::is_regular_file(path, ec)) {
        recordInstallLog(
            QStringLiteral("asset-toggle"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            false,
            QStringLiteral("Asset not found: %1").arg(assetPath),
            QStringLiteral("missing-asset"));
        return false;
    }

    const auto filename = path.filename().generic_string();
    const auto suffix = std::string(".disabled");
    const auto hasSuffix = filename.size() > suffix.size() && filename.rfind(suffix) == filename.size() - suffix.size();

    std::filesystem::path targetPath = path;
    if (enabled) {
        if (!hasSuffix) {
            recordInstallLog(
                QStringLiteral("asset-toggle"),
                QStringLiteral("instance_asset"),
                activeInstanceId(),
                true,
                QStringLiteral("Asset already enabled: %1").arg(assetPath),
                QStringLiteral("no-op"));
            return true;
        }
        const auto restoredName = filename.substr(0, filename.size() - suffix.size());
        targetPath = path.parent_path() / restoredName;
    } else {
        if (hasSuffix) {
            recordInstallLog(
                QStringLiteral("asset-toggle"),
                QStringLiteral("instance_asset"),
                activeInstanceId(),
                true,
                QStringLiteral("Asset already disabled: %1").arg(assetPath),
                QStringLiteral("no-op"));
            return true;
        }
        targetPath = path.parent_path() / (filename + suffix);
    }

    std::filesystem::rename(path, targetPath, ec);
    if (ec) {
        recordInstallLog(
            QStringLiteral("asset-toggle"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            false,
            QStringLiteral("Failed to toggle asset: %1").arg(assetPath),
            QStringLiteral("rename-failed"));
        return false;
    }

    recordInstallLog(
        QStringLiteral("asset-toggle"),
        QStringLiteral("instance_asset"),
        activeInstanceId(),
        true,
        QStringLiteral("%1 asset: %2").arg(enabled ? QStringLiteral("Enabled") : QStringLiteral("Disabled"), assetPath),
        enabled ? QStringLiteral("asset-enabled") : QStringLiteral("asset-disabled"));
    refresh();
    return true;
}

bool AppViewModel::removeAsset(const QString& assetPath) {
    if (assetPath.trimmed().isEmpty()) {
        recordInstallLog(
            QStringLiteral("asset-remove"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            false,
            QStringLiteral("Asset path is empty"),
            QStringLiteral("invalid-path"));
        return false;
    }

    const std::filesystem::path path = to_std(assetPath);
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        recordInstallLog(
            QStringLiteral("asset-remove"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            true,
            QStringLiteral("Asset already removed: %1").arg(assetPath),
            QStringLiteral("no-op"));
        return true;
    }

    if (std::filesystem::is_directory(path, ec)) {
        std::filesystem::remove_all(path, ec);
    } else {
        std::filesystem::remove(path, ec);
    }
    if (ec) {
        recordInstallLog(
            QStringLiteral("asset-remove"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            false,
            QStringLiteral("Failed to remove asset: %1").arg(assetPath),
            QStringLiteral("remove-failed"));
        return false;
    }

    recordInstallLog(
        QStringLiteral("asset-remove"),
        QStringLiteral("instance_asset"),
        activeInstanceId(),
        true,
        QStringLiteral("Removed asset: %1").arg(assetPath),
        QStringLiteral("asset-removed"));
    refresh();
    return true;
}

bool AppViewModel::openPath(const QString& path) {
    if (path.trimmed().isEmpty()) {
        recordInstallLog(
            QStringLiteral("asset-open"),
            QStringLiteral("instance_asset"),
            activeInstanceId(),
            false,
            QStringLiteral("Path is empty"),
            QStringLiteral("invalid-path"));
        return false;
    }
    const bool opened = QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    recordInstallLog(
        QStringLiteral("asset-open"),
        QStringLiteral("instance_asset"),
        activeInstanceId(),
        opened,
        opened ? QStringLiteral("Opened path: %1").arg(path) : QStringLiteral("Failed to open path: %1").arg(path),
        opened ? QStringLiteral("path-opened") : QStringLiteral("open-failed"));
    return opened;
}

int AppViewModel::setAllAssetsEnabled(const QString& assetType, bool enabled) {
    if (!is_togglable_asset_type(assetType)) {
        return 0;
    }
    if (instances_.empty()) {
        return 0;
    }

    const auto index = activeInstanceIndex();
    if (index >= instances_.size()) {
        return 0;
    }
    const auto folder = asset_directory_path(instances_[index].gameDir, assetType);
    std::error_code ec;
    if (folder.empty() || !std::filesystem::exists(folder, ec) || !std::filesystem::is_directory(folder, ec)) {
        return 0;
    }

    int changed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        if (toggleAssetEnabled(assetType, to_qstring(entry.path().generic_string()), enabled)) {
            ++changed;
        }
    }
    refresh();
    return changed;
}

int AppViewModel::removeDisabledAssets(const QString& assetType) {
    if (!is_togglable_asset_type(assetType)) {
        return 0;
    }
    if (instances_.empty()) {
        return 0;
    }

    const auto index = activeInstanceIndex();
    if (index >= instances_.size()) {
        return 0;
    }
    const auto folder = asset_directory_path(instances_[index].gameDir, assetType);
    std::error_code ec;
    if (folder.empty() || !std::filesystem::exists(folder, ec) || !std::filesystem::is_directory(folder, ec)) {
        return 0;
    }

    constexpr auto kDisabledSuffix = ".disabled";
    int removed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (ec || !entry.is_regular_file()) {
            continue;
        }
        const auto name = entry.path().filename().generic_string();
        const auto hasSuffix = name.size() > std::string(kDisabledSuffix).size()
            && name.rfind(kDisabledSuffix) == name.size() - std::string(kDisabledSuffix).size();
        if (!hasSuffix) {
            continue;
        }
        if (removeAsset(to_qstring(entry.path().generic_string()))) {
            ++removed;
        }
    }
    refresh();
    return removed;
}

int AppViewModel::removeAllAssets(const QString& assetType) {
    if (!is_supported_asset_type(assetType)) {
        return 0;
    }
    if (instances_.empty()) {
        return 0;
    }

    const auto index = activeInstanceIndex();
    if (index >= instances_.size()) {
        return 0;
    }
    const auto folder = asset_directory_path(instances_[index].gameDir, assetType);
    std::error_code ec;
    if (folder.empty() || !std::filesystem::exists(folder, ec) || !std::filesystem::is_directory(folder, ec)) {
        return 0;
    }

    int removed = 0;
    for (const auto& entry : std::filesystem::directory_iterator(folder, ec)) {
        if (ec) {
            continue;
        }
        if (removeAsset(to_qstring(entry.path().generic_string()))) {
            ++removed;
        }
    }
    refresh();
    return removed;
}

void AppViewModel::updateSelectedContentPreview() {
    refreshInstallPreview(false);
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

void AppViewModel::setAutoCreatedInstanceNotice(const QString& instanceId, const QString& sourceLabel) {
    if (instanceId.isEmpty()) {
        return;
    }
    autoCreatedInstanceId_ = instanceId;
    const auto label = sourceLabel.isEmpty() ? QStringLiteral("modpack") : sourceLabel;
    autoCreatedInstanceNoticeText_ = QStringLiteral("Installed %1 into new instance: %2").arg(label, instanceId);
}

void AppViewModel::recordInstallLog(const QString& type, const QString& sourceType, const QString& targetInstanceId, bool success, const QString& summary, const QString& result, const QString& projectId, const QString& versionId) {
    InstallLogEntry entry;
    entry.eventId = QStringLiteral("event-%1-%2").arg(current_timestamp_text()).arg(++eventSequence_);
    entry.eventType = normalize_event_type(type, sourceType);
    entry.time = current_timestamp_text();
    entry.type = type;
    entry.sourceType = sourceType;
    entry.targetInstanceId = targetInstanceId;
    entry.projectId = projectId;
    entry.versionId = versionId;
    entry.success = success;
    entry.summary = summary;
    entry.result = result.isEmpty() ? (success ? QStringLiteral("success") : QStringLiteral("failure")) : result;
    entry.pageHint = page_hint_for_event(entry.eventType, targetInstanceId, projectId);
    entry.pageIndex = page_index_for_hint(entry.pageHint);

    installLogs_.push_back(std::move(entry));
    constexpr std::size_t kMaxInstallLogs = 30;
    if (installLogs_.size() > kMaxInstallLogs) {
        const auto removeCount = installLogs_.size() - kMaxInstallLogs;
        installLogs_.erase(installLogs_.begin(), installLogs_.begin() + static_cast<std::vector<InstallLogEntry>::difference_type>(removeCount));
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
    refreshInstallPreview(false);
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
    if (!autoCreatedInstanceId_.isEmpty()) {
        const auto it = std::find_if(instances_.begin(), instances_.end(), [&](const dawn::core::InstanceManifest& manifest) {
            return to_qstring(manifest.id) == autoCreatedInstanceId_;
        });
        if (it == instances_.end()) {
            autoCreatedInstanceId_.clear();
            autoCreatedInstanceNoticeText_.clear();
        }
    }
    refreshInstallPreview(false);
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

QVariantMap AppViewModel::installLogToVariant(const InstallLogEntry& entry) const {
    return QVariantMap{
        {"eventId", entry.eventId},
        {"eventType", entry.eventType},
        {"instanceId", entry.targetInstanceId},
        {"time", entry.time},
        {"type", entry.type},
        {"sourceType", entry.sourceType},
        {"targetInstanceId", entry.targetInstanceId},
        {"projectId", entry.projectId},
        {"versionId", entry.versionId},
        {"result", entry.result},
        {"summary", entry.summary},
        {"pageHint", entry.pageHint},
        {"pageIndex", entry.pageIndex},
        {"eventTargetPage", entry.pageHint},
        {"eventTargetInstanceId", entry.targetInstanceId},
        {"eventTargetProjectId", entry.projectId},
        {"success", entry.success},
        {"selected", entry.selected},
    };
}

QVariantMap AppViewModel::eventCenterContextToVariant(const InstallLogEntry& entry) const {
    return QVariantMap{
        {"eventId", entry.eventId},
        {"eventType", entry.eventType},
        {"type", entry.type},
        {"sourceType", entry.sourceType},
        {"instanceId", entry.targetInstanceId},
        {"targetInstanceId", entry.targetInstanceId},
        {"projectId", entry.projectId},
        {"versionId", entry.versionId},
        {"status", entry.result},
        {"time", entry.time},
        {"summary", entry.summary},
        {"pageHint", entry.pageHint},
        {"pageIndex", entry.pageIndex},
        {"eventTargetPage", entry.pageHint},
        {"eventTargetInstanceId", entry.targetInstanceId},
        {"eventTargetProjectId", entry.projectId},
        {"success", entry.success},
        {"selected", entry.selected},
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
