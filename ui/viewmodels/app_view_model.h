#pragma once

#include "dawn/core/model/instance_manifest.h"
#include "dawn/core/model/instance_workbench.h"
#include "dawn/core/model/preflight.h"
#include "dawn/core/model/task_types.h"
#include "dawn/core/service/instance_service.h"
#include "dawn/core/service/preflight_service.h"
#include "dawn/core/service/task_queue.h"

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <cstddef>
#include <vector>

namespace dawn::ui {

class AppViewModel final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList instanceCards READ instanceCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList taskCards READ taskCards NOTIFY dataChanged)
    Q_PROPERTY(QVariantList instanceWorkbenchTabs READ instanceWorkbenchTabs NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap activeInstanceWorkbench READ activeInstanceWorkbench NOTIFY dataChanged)
    Q_PROPERTY(QVariantMap primaryPreflight READ primaryPreflight NOTIFY dataChanged)
    Q_PROPERTY(QString primaryInstanceId READ primaryInstanceId NOTIFY dataChanged)
    Q_PROPERTY(QString activeInstanceId READ activeInstanceId NOTIFY dataChanged)
    Q_PROPERTY(QString activeInstanceTabId READ activeInstanceTabId NOTIFY dataChanged)
    Q_PROPERTY(int instanceCount READ instanceCount NOTIFY dataChanged)
    Q_PROPERTY(int taskCount READ taskCount NOTIFY dataChanged)
    Q_PROPERTY(QString dataRoot READ dataRoot CONSTANT)

public:
    explicit AppViewModel(QString dataRoot, QObject* parent = nullptr);

    [[nodiscard]] QVariantList instanceCards() const;
    [[nodiscard]] QVariantList taskCards() const;
    [[nodiscard]] QVariantList instanceWorkbenchTabs() const;
    [[nodiscard]] QVariantMap activeInstanceWorkbench() const;
    [[nodiscard]] QVariantMap primaryPreflight() const;
    [[nodiscard]] QString primaryInstanceId() const;
    [[nodiscard]] QString activeInstanceId() const;
    [[nodiscard]] QString activeInstanceTabId() const;
    [[nodiscard]] int instanceCount() const;
    [[nodiscard]] int taskCount() const;
    [[nodiscard]] QString dataRoot() const;

    Q_INVOKABLE bool createInstance(const QString& name, const QString& mcVersion, const QString& loaderType = QStringLiteral("none"));
    Q_INVOKABLE bool enqueueDemoTask(const QString& title);
    Q_INVOKABLE QVariantMap preflightFor(const QString& instanceId) const;
    Q_INVOKABLE void setActiveInstance(const QString& instanceId);
    Q_INVOKABLE void setActiveInstanceTab(const QString& tabId);
    Q_INVOKABLE void refresh();

signals:
    void dataChanged();

private:
    [[nodiscard]] QVariantMap instanceToVariant(const dawn::core::InstanceManifest& manifest) const;
    [[nodiscard]] QVariantMap taskToVariant(const dawn::core::TaskPlan& plan) const;
    [[nodiscard]] QVariantMap preflightToVariant(const dawn::core::PreflightResult& result) const;
    [[nodiscard]] QVariantMap workbenchToVariant(const dawn::core::InstanceWorkbenchState& workbench) const;
    [[nodiscard]] std::size_t resourceCount(const dawn::core::InstanceManifest& manifest) const;
    [[nodiscard]] std::size_t activeInstanceIndex() const;

    QString dataRoot_;
    dawn::core::InstanceService instanceService_;
    dawn::core::PreflightService preflightService_;
    dawn::core::TaskQueue taskQueue_;
    std::vector<dawn::core::InstanceManifest> instances_;
    QString activeInstanceId_;
    QString activeInstanceTabId_ = QStringLiteral("overview");
};

} // namespace dawn::ui
