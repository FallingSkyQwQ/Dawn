#pragma once

#include "dawn/core/model/instance_manifest.h"
#include "dawn/core/model/instance_workbench.h"
#include "dawn/core/model/preflight.h"
#include "dawn/core/model/task_types.h"
#include "dawn/core/content/content_install_service.h"
#include "dawn/core/service/instance_service.h"
#include "dawn/core/service/preflight_service.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/core/download/download_service.h"
#include "dawn/core/provider/modrinth_provider.h"
#include "dawn/core/settings/settings_service.h"
#include "dawn/infra/net/http_client.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <cstddef>
#include <memory>
#include <optional>
#include <vector>

namespace dawn::ui {

class AppViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList instanceCards READ instanceCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList taskCards READ taskCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList contentSearchResults READ contentSearchResults NOTIFY dataChanged)
    Q_PROPERTY(QVariantList contentVersions READ contentVersions NOTIFY dataChanged)
    Q_PROPERTY(QVariantList instanceWorkbenchTabs READ instanceWorkbenchTabs NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap activeInstanceWorkbench READ activeInstanceWorkbench NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap primaryPreflight READ primaryPreflight NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap installPreview READ installPreview NOTIFY dataChanged)
    Q_PROPERTY(QVariantList installDiagnostics READ installDiagnostics NOTIFY dataChanged)
    Q_PROPERTY(QVariantList rollbackEvents READ rollbackEvents NOTIFY dataChanged)
    Q_PROPERTY(QVariantList installLogs READ installLogs NOTIFY dataChanged)
    Q_PROPERTY(QString installLogFilter READ installLogFilter WRITE setInstallLogFilter NOTIFY dataChanged)
    Q_PROPERTY(QVariantList repairExecutionLogs READ repairExecutionLogs NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap lastDroppedFileResult READ lastDroppedFileResult NOTIFY dataChanged)
    Q_PROPERTY(QVariantList wizardSteps READ wizardSteps NOTIFY dataChanged)
    Q_PROPERTY(int wizardStepIndex READ wizardStepIndex NOTIFY dataChanged)
    Q_PROPERTY(QString installPreviewStatus READ installPreviewStatus NOTIFY dataChanged)
    Q_PROPERTY(QString repairExecutionStatus READ repairExecutionStatus NOTIFY dataChanged)
    Q_PROPERTY(bool firstLaunchCompleted READ firstLaunchCompleted NOTIFY dataChanged)
    Q_PROPERTY(bool firstLaunchVisible READ firstLaunchVisible NOTIFY dataChanged)
    Q_PROPERTY(QString uiMode READ uiMode WRITE setUiMode NOTIFY dataChanged)
    Q_PROPERTY(QString javaStrategy READ javaStrategy NOTIFY dataChanged)
    Q_PROPERTY(QString cachePath READ cachePath NOTIFY dataChanged)
    Q_PROPERTY(int lowDiskThresholdGb READ lowDiskThresholdGb WRITE setLowDiskThresholdGb NOTIFY dataChanged)
    Q_PROPERTY(QString lowDiskWarning READ lowDiskWarning NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap diskSpaceStatus READ diskSpaceStatus NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap cacheCleanupSummary READ cacheCleanupSummary NOTIFY dataChanged)
    Q_PROPERTY(QString selectedContentProjectId READ selectedContentProjectId NOTIFY dataChanged)
    Q_PROPERTY(QString selectedContentVersionId READ selectedContentVersionId NOTIFY dataChanged)
    Q_PROPERTY(QString selectedTargetInstanceId READ selectedTargetInstanceId NOTIFY dataChanged)
    Q_PROPERTY(QString primaryInstanceId READ primaryInstanceId NOTIFY dataChanged)
    Q_PROPERTY(QString activeInstanceId READ activeInstanceId NOTIFY dataChanged)
    Q_PROPERTY(QString activeInstanceTabId READ activeInstanceTabId NOTIFY dataChanged)
    Q_PROPERTY(int instanceCount READ instanceCount NOTIFY dataChanged)
    Q_PROPERTY(int taskCount READ taskCount NOTIFY dataChanged)
    Q_PROPERTY(QString dataRoot READ dataRoot CONSTANT)

public:
    explicit AppViewModel(QString dataRoot, QObject* parent = nullptr);
    AppViewModel(QString dataRoot, std::shared_ptr<dawn::core::IContentProvider> contentProvider, QObject* parent = nullptr);
    AppViewModel(QString dataRoot, std::shared_ptr<dawn::core::IContentProvider> contentProvider, std::shared_ptr<dawn::infra::net::HttpClient> downloadClient, QObject* parent = nullptr);

    [[nodiscard]] QVariantList instanceCards() const;
    [[nodiscard]] QVariantList taskCards() const;
    [[nodiscard]] QVariantList contentSearchResults() const;
    [[nodiscard]] QVariantList contentVersions() const;
    [[nodiscard]] QVariantList instanceWorkbenchTabs() const;
    [[nodiscard]] QVariantMap activeInstanceWorkbench() const;
    [[nodiscard]] QVariantMap primaryPreflight() const;
    [[nodiscard]] QVariantMap installPreview() const;
    [[nodiscard]] QVariantList installDiagnostics() const;
    [[nodiscard]] QVariantList rollbackEvents() const;
    [[nodiscard]] QVariantList installLogs() const;
    [[nodiscard]] QString installLogFilter() const;
    [[nodiscard]] QVariantList repairExecutionLogs() const;
    [[nodiscard]] QVariantMap lastDroppedFileResult() const;
    [[nodiscard]] QVariantList wizardSteps() const;
    [[nodiscard]] int wizardStepIndex() const;
    [[nodiscard]] QString installPreviewStatus() const;
    [[nodiscard]] QString repairExecutionStatus() const;
    [[nodiscard]] bool firstLaunchCompleted() const;
    [[nodiscard]] bool firstLaunchVisible() const;
    [[nodiscard]] QString uiMode() const;
    [[nodiscard]] QString javaStrategy() const;
    [[nodiscard]] QString cachePath() const;
    [[nodiscard]] int lowDiskThresholdGb() const;
    [[nodiscard]] QString lowDiskWarning() const;
    [[nodiscard]] QVariantMap diskSpaceStatus() const;
    [[nodiscard]] QVariantMap cacheCleanupSummary() const;
    [[nodiscard]] QString selectedContentProjectId() const;
    [[nodiscard]] QString selectedContentVersionId() const;
    [[nodiscard]] QString selectedTargetInstanceId() const;
    [[nodiscard]] QString primaryInstanceId() const;
    [[nodiscard]] QString activeInstanceId() const;
    [[nodiscard]] QString activeInstanceTabId() const;
    [[nodiscard]] int instanceCount() const;
    [[nodiscard]] int taskCount() const;
    [[nodiscard]] QString dataRoot() const;

    Q_INVOKABLE bool createInstance(const QString& name, const QString& mcVersion, const QString& loaderType = QStringLiteral("none"));
    Q_INVOKABLE bool enqueueDemoTask(const QString& title);
    Q_INVOKABLE bool searchContent(const QString& text, const QString& projectType = QStringLiteral("mod"));
    Q_INVOKABLE bool selectSearchResult(const QString& projectId);
    Q_INVOKABLE bool selectTargetInstance(const QString& instanceId);
    Q_INVOKABLE bool selectInstallVersion(const QString& versionId);
    Q_INVOKABLE void refreshInstallPreview();
    Q_INVOKABLE bool nextWizardStep();
    Q_INVOKABLE bool previousWizardStep();
    Q_INVOKABLE bool executeRepairPlan();
    Q_INVOKABLE bool executeRepairPlan(const QString& planId);
    Q_INVOKABLE bool executeRepairPlan(int planIndex);
    Q_INVOKABLE bool completeFirstLaunch();
    Q_INVOKABLE void setInstallLogFilter(const QString& filter);
    Q_INVOKABLE QVariantMap handleDroppedFile(const QString& path, const QString& instanceId);
    Q_INVOKABLE void setUiMode(const QString& mode);
    Q_INVOKABLE void setJavaStrategy(const QString& strategy);
    Q_INVOKABLE void setLowDiskThresholdGb(int thresholdGb);
    Q_INVOKABLE bool clearCache();
    Q_INVOKABLE QVariantMap preflightFor(const QString& instanceId) const;
    Q_INVOKABLE void setActiveInstance(const QString& instanceId);
    Q_INVOKABLE void setActiveInstanceTab(const QString& tabId);
    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    struct InstallLogEntry;
    [[nodiscard]] QVariantMap instanceToVariant(const dawn::core::InstanceManifest& manifest) const;
    [[nodiscard]] QVariantMap taskToVariant(const dawn::core::TaskPlan& plan) const;
    [[nodiscard]] QVariantMap stepToVariant(const dawn::core::TaskStep& step) const;
    [[nodiscard]] QVariantMap preflightToVariant(const dawn::core::PreflightResult& result) const;
    [[nodiscard]] QVariantMap workbenchToVariant(const dawn::core::InstanceWorkbenchState& workbench) const;
    [[nodiscard]] QVariantMap contentSearchResultToVariant(const dawn::core::SearchResultItem& item) const;
    [[nodiscard]] QVariantMap contentVersionToVariant(const dawn::core::ContentVersion& version) const;
    [[nodiscard]] QVariantMap versionSuggestionToVariant(const dawn::core::VersionSuggestion& suggestion) const;
    [[nodiscard]] QVariantMap dependencyTreeToVariant(const dawn::core::DependencyTreeNode& node) const;
    [[nodiscard]] QVariantMap repairPlanToVariant(const dawn::core::TaskPlan& plan, bool available) const;
    [[nodiscard]] QVariantMap diagnosticToVariant(const dawn::core::InstallDiagnostic& diagnostic) const;
    [[nodiscard]] QVariantMap rollbackEventToVariant(const dawn::core::ContentInstallResult::RollbackEvent& event) const;
    [[nodiscard]] QVariantMap installLogToVariant(const InstallLogEntry& entry) const;
    [[nodiscard]] QVariantMap diskStatusToVariant(const dawn::core::DiskSpaceCheckResult& result) const;
    [[nodiscard]] QVariantMap cacheCleanupToVariant(const dawn::core::CacheCleanupResult& result) const;
    [[nodiscard]] std::size_t resourceCount(const dawn::core::InstanceManifest& manifest) const;
    [[nodiscard]] std::size_t activeInstanceIndex() const;
    [[nodiscard]] std::optional<dawn::core::InstallRequest> currentInstallRequest() const;
    void refreshSettingsState();
    void persistSettings();
    void refreshDiskStatus();
    void updateSelectedContentPreview();
    void refreshSelectedContentVersions();
    void populateSearchResults(const dawn::core::SearchResult& result);
    void recordInstallLog(const QString& type, const QString& targetInstanceId, bool success, const QString& summary, const QString& result = QString());

    QString dataRoot_;
    dawn::core::SettingsService settingsService_;
    dawn::core::GlobalSettings settings_;
    dawn::core::DiskSpaceCheckResult diskSpaceStatus_;
    dawn::core::CacheCleanupResult cacheCleanupSummary_;
    dawn::core::InstanceService instanceService_;
    dawn::core::PreflightService preflightService_;
    dawn::core::TaskQueue taskQueue_;
    dawn::core::DownloadService previewDownloadService_;
    dawn::core::ContentInstallService contentInstallService_;
    std::shared_ptr<dawn::core::IContentProvider> contentProvider_;
    dawn::core::SearchQuery contentSearchQuery_;
    std::vector<dawn::core::SearchResultItem> contentSearchResults_;
    std::vector<dawn::core::ContentVersion> contentVersions_;
    std::vector<dawn::core::InstallDiagnostic> installDiagnostics_;
    std::vector<dawn::core::ContentInstallResult::RollbackEvent> rollbackEvents_;
    struct InstallLogEntry {
        QString time;
        QString type;
        QString targetInstanceId;
        QString result;
        QString summary;
        bool success = false;
    };
    std::vector<InstallLogEntry> installLogs_;
    QString installLogFilter_ = QStringLiteral("all");
    std::vector<std::string> repairExecutionLogs_;
    QVariantMap lastDroppedFileResult_;
    dawn::core::DependencyCheckResult installPreview_;
    QString installPreviewStatus_ = QStringLiteral("No install preview run");
    QString repairExecutionStatus_ = QStringLiteral("No repair run");
    int wizardStepIndex_ = 0;
    QString selectedContentProjectId_;
    QString selectedContentVersionId_;
    std::vector<dawn::core::InstanceManifest> instances_;
    QString activeInstanceId_;
    QString activeInstanceTabId_ = QStringLiteral("overview");
};

} // namespace dawn::ui
