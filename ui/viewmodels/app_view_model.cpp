#include "app_view_model.h"

#include "dawn/core/model/instance_workbench.h"
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

class DemoPreviewProvider final : public dawn::core::IContentProvider {
public:
    DemoPreviewProvider() {
        dawn::core::ContentVersion version;
        version.versionId = "1.0.0";
        version.name = "Demo Content";
        version.fileUrls = {"https://example.invalid/demo-content.jar"};
        version.loaders = {dawn::core::LoaderType::Forge};
        version.dependencies = {
            {"required-lib", {}, {}, dawn::core::DependencyRequirement::Required, "required for rendering"},
            {"optional-lib", {}, {}, dawn::core::DependencyRequirement::Optional, "optional enhancement"},
            {"bad-lib", {}, {}, dawn::core::DependencyRequirement::Incompatible, "known conflict"},
        };
        versions_.push_back(version);
        dependencyGraph_.dependencies = version.dependencies;
    }

    dawn::core::SearchResult search(const dawn::core::SearchQuery&) override {
        return {};
    }

    std::vector<dawn::core::ContentVersion> versions(const std::string&) override {
        return versions_;
    }

    dawn::core::DependencyGraph resolveDependencies(const dawn::core::InstallRequest&) override {
        return dependencyGraph_;
    }

    dawn::core::TaskPlan buildInstallPlan(const dawn::core::InstallRequest& request) override {
        dawn::core::TaskPlan plan;
        plan.id = "preview-" + request.projectId;
        plan.title = "Preview " + request.projectId;
        plan.steps = {
            {"resolve", "Resolve", dawn::core::TaskStatus::Pending, 0, {}},
            {"download", "Download", dawn::core::TaskStatus::Pending, 0, {}},
            {"deploy", "Deploy", dawn::core::TaskStatus::Pending, 0, {}},
        };
        return plan;
    }

private:
    std::vector<dawn::core::ContentVersion> versions_;
    dawn::core::DependencyGraph dependencyGraph_;
};

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

} // namespace

AppViewModel::AppViewModel(QString dataRoot, QObject* parent)
    : QObject(parent)
    , dataRoot_(std::move(dataRoot))
    , instanceService_(std::filesystem::path(dataRoot_.toStdString()))
    , previewDownloadService_(std::make_shared<dawn::infra::net::FakeHttpClient>())
    , contentInstallService_(std::filesystem::path(dataRoot_.toStdString()), previewDownloadService_)
{
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

void AppViewModel::refreshInstallPreview() {
    installDiagnostics_.clear();
    rollbackEvents_.clear();
    installPreviewStatus_ = QStringLiteral("No install preview available");

    const auto instanceId = activeInstanceId();
    if (instanceId.isEmpty()) {
        installPreviewStatus_ = QStringLiteral("Create or select an instance to preview installs");
        emit dataChanged();
        return;
    }

    DemoPreviewProvider provider;
    dawn::core::InstallRequest request;
    request.provider = "modrinth";
    request.instanceId = to_std(instanceId);
    request.projectId = "demo-content";
    request.versionId = "1.0.0";
    request.projectType = dawn::core::ProjectType::Mod;

    const auto result = contentInstallService_.install(request, provider, nullptr);
    for (const auto& diagnostic : result.diagnostics) {
        installDiagnostics_.push_back(diagnostic);
    }
    for (const auto& event : result.rollbackEvents) {
        rollbackEvents_.push_back(event);
    }
    installPreviewStatus_ = result.success
        ? QStringLiteral("Install preview succeeded")
        : QString::fromStdString(result.message.empty() ? "Install preview blocked" : result.message);
    emit dataChanged();
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
