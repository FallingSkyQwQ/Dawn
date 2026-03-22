import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel
    property string lastShownAutoCreatedInstanceId: ""
    property string pendingDeletePath: ""
    property string pendingDeleteAssetType: ""

    function tabIndex(tabId) {
        var tabs = appViewModel.activeInstanceWorkbench.tabs || []
        for (var i = 0; i < tabs.length; ++i) {
            if (tabs[i].id === tabId) {
                return i
            }
        }
        return 0
    }

    function instanceTableRows() {
        var rows = []
        var data = appViewModel ? appViewModel.instanceCards : []
        for (var i = 0; i < data.length; ++i) {
            var item = data[i]
            rows.push({
                "_key": item.id || ("instance-" + i),
                "id": item.id || "",
                "name": item.name || "",
                "mcVersion": item.mcVersion || "",
                "loader": item.loader || "",
                "health": item.health || "",
                "resourceCount": item.resourceCount || 0,
                "javaProfileId": item.javaProfileId || "",
                "selected": item.selected ? "yes" : "",
                "openAction": instanceTable.customItem(comOpenInstanceAction, { "instanceId": item.id || "" })
            })
        }
        return rows
    }

    function activePreflight() {
        if (!appViewModel || appViewModel.activeInstanceId.length === 0) {
            return { "ready": false, "issues": [] }
        }
        return appViewModel.preflightFor(appViewModel.activeInstanceId)
    }

    function assetList(assetKey) {
        if (!appViewModel || !appViewModel.activeInstanceAssets) {
            return []
        }
        var data = appViewModel.activeInstanceAssets[assetKey]
        return data ? data : []
    }

    function workbenchAssetKey(tabId) {
        switch (tabId) {
        case "mods": return "mods"
        case "resourcepacks": return "resourcepacks"
        case "shaderpacks": return "shaderpacks"
        case "worlds": return "worlds"
        case "logs": return "logs"
        default: return ""
        }
    }

    function workbenchRows(tabId, tableRef) {
        var key = workbenchAssetKey(tabId)
        if (key.length === 0) {
            return []
        }
        var src = assetList(key)
        var rows = []
        for (var i = 0; i < src.length; ++i) {
            var item = src[i]
            if (tabId === "worlds") {
                rows.push({
                    "_key": item.path || ("world-" + i),
                    "name": item.name || "",
                    "path": item.path || "",
                    "openAction": tableRef.customItem(comOpenAssetAction, { "path": item.path || "" }),
                    "deleteAction": tableRef.customItem(comDeleteAssetAction, { "path": item.path || "", "assetType": tabId })
                })
            } else {
                var enabled = item.enabled === undefined ? true : !!item.enabled
                var canToggle = tabId === "mods" || tabId === "resourcepacks" || tabId === "shaderpacks"
                rows.push({
                    "_key": item.path || ("asset-" + i),
                    "name": item.name || "",
                    "status": item.status || (enabled ? "Enabled" : "Disabled"),
                    "sizeDisplay": item.sizeDisplay || "",
                    "path": item.path || "",
                    "toggleAction": canToggle
                        ? tableRef.customItem(comToggleAssetAction, {
                            "assetType": tabId,
                            "path": item.path || "",
                            "enabled": enabled
                        })
                        : "",
                    "openAction": tableRef.customItem(comOpenAssetAction, { "path": item.path || "" }),
                    "deleteAction": tableRef.customItem(comDeleteAssetAction, { "path": item.path || "", "assetType": tabId })
                })
            }
        }
        return rows
    }

    function workbenchColumns(tabId) {
        if (tabId === "worlds") {
            return [
                { "title": "World", "dataIndex": "name", "width": 240 },
                { "title": "Path", "dataIndex": "path", "width": 420 },
                { "title": "Open", "dataIndex": "openAction", "width": 80 },
                { "title": "Delete", "dataIndex": "deleteAction", "width": 80 }
            ]
        }
        if (tabId === "mods" || tabId === "resourcepacks" || tabId === "shaderpacks") {
            return [
                { "title": "Name", "dataIndex": "name", "width": 200 },
                { "title": "State", "dataIndex": "status", "width": 90 },
                { "title": "Size", "dataIndex": "sizeDisplay", "width": 100 },
                { "title": "Path", "dataIndex": "path", "width": 260 },
                { "title": "Toggle", "dataIndex": "toggleAction", "width": 80 },
                { "title": "Open", "dataIndex": "openAction", "width": 80 },
                { "title": "Delete", "dataIndex": "deleteAction", "width": 80 }
            ]
        }
        if (tabId === "logs") {
            return [
                { "title": "Name", "dataIndex": "name", "width": 220 },
                { "title": "Size", "dataIndex": "sizeDisplay", "width": 100 },
                { "title": "Path", "dataIndex": "path", "width": 300 },
                { "title": "Open", "dataIndex": "openAction", "width": 80 },
                { "title": "Delete", "dataIndex": "deleteAction", "width": 80 }
            ]
        }
        return []
    }

    function isTogglableTab(tabId) {
        return tabId === "mods" || tabId === "resourcepacks" || tabId === "shaderpacks"
    }

    function isRemovableTab(tabId) {
        return isTogglableTab(tabId) || tabId === "logs" || tabId === "worlds"
    }

    FluContentDialog {
        id: deleteAssetDialog
        title: "Delete Asset"
        message: "Remove selected asset from instance storage?"
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        negativeText: "Cancel"
        positiveText: "Delete"
        onPositiveClicked: {
            if (root.pendingDeletePath.length === 0) {
                return
            }
            var ok = root.appViewModel.removeAsset(root.pendingDeletePath)
            var win = root.Window.window
            if (win && win.showSuccess && win.showError) {
                if (ok) {
                    win.showSuccess("Asset removed", 2200, root.pendingDeletePath)
                } else {
                    win.showError("Asset removal failed", 2800, root.pendingDeletePath)
                }
            }
            root.pendingDeletePath = ""
            root.pendingDeleteAssetType = ""
        }
    }

    function workbenchItemCount(tabId) {
        var key = workbenchAssetKey(tabId)
        if (key.length === 0) {
            return 0
        }
        return assetList(key).length
    }

    Component {
        id: comToggleAssetAction
        Item {
            FluButton {
                anchors.centerIn: parent
                text: (options && options.enabled) ? "Disable" : "Enable"
                onClicked: {
                    if (!options || !options.path) {
                        return
                    }
                    var ok = root.appViewModel.toggleAssetEnabled(options.assetType || "", options.path, !(options.enabled === true))
                    var win = root.Window.window
                    if (win && win.showSuccess && win.showError) {
                        if (ok) {
                            win.showSuccess("Asset state updated", 2200, options.path)
                        } else {
                            win.showError("Asset state update failed", 2800, options.path)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: comOpenAssetAction
        Item {
            FluButton {
                anchors.centerIn: parent
                text: "Open"
                onClicked: {
                    if (!options || !options.path) {
                        return
                    }
                    var ok = root.appViewModel.openPath(options.path)
                    if (!ok) {
                        var win = root.Window.window
                        if (win && win.showError) {
                            win.showError("Open path failed", 2400, options.path)
                        }
                    }
                }
            }
        }
    }

    Component {
        id: comDeleteAssetAction
        Item {
            FluButton {
                anchors.centerIn: parent
                text: "Delete"
                onClicked: {
                    if (!options || !options.path) {
                        return
                    }
                    root.pendingDeletePath = options.path
                    root.pendingDeleteAssetType = options.assetType || ""
                    deleteAssetDialog.open()
                }
            }
        }
    }

    function showAutoCreatedInstanceInfoBar() {
        if (!root.appViewModel || !root.appViewModel.autoCreatedInstanceNoticeVisible) {
            return
        }
        var instanceId = root.appViewModel.autoCreatedInstanceId || ""
        if (instanceId.length === 0 || instanceId === root.lastShownAutoCreatedInstanceId) {
            return
        }
        var win = root.Window.window
        if (win && win.showSuccess) {
            win.showSuccess(root.appViewModel.autoCreatedInstanceNoticeText, 3600, "Open the instance workbench from this page.")
        }
        root.lastShownAutoCreatedInstanceId = instanceId
        root.appViewModel.clearAutoCreatedInstanceNotice()
    }

    Connections {
        target: root.appViewModel
        function onDataChanged() {
            root.showAutoCreatedInstanceInfoBar()
        }
    }

    Component.onCompleted: {
        root.showAutoCreatedInstanceInfoBar()
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: layout.implicitHeight
        clip: true

        ColumnLayout {
            id: layout
            width: parent.width
            spacing: 18

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 170
                title: "Instances"
                subtitle: "Instance-centered management, launch readiness, and workbench tabs."

                RowLayout {
                    anchors.fill: parent
                    spacing: 12

                    FluFilledButton {
                        text: "Create Vanilla Instance"
                        onClicked: appViewModel.createInstance("Dawn Vanilla", "1.20.1", "none")
                    }

                    FluFilledButton {
                        text: "Create Fabric Instance"
                        onClicked: appViewModel.createInstance("Dawn Fabric", "1.20.1", "fabric")
                    }

                    FluButton {
                        text: "Refresh"
                        onClicked: appViewModel.refresh()
                    }

                    Item { Layout.fillWidth: true }

                    Column {
                        spacing: 4

                        FluText {
                            text: appViewModel.activeInstanceId.length > 0 ? "Active: " + appViewModel.activeInstanceId : "No active instance"
                            color: "#f5f8fb"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        FluText {
                            text: appViewModel.activeInstanceWorkbench.instanceName.length > 0 ? appViewModel.activeInstanceWorkbench.instanceName : "Select an instance card to open its workbench"
                            color: "#9eb0c7"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            EventCenterPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 340
                title: "Event Center"
                subtitle: "Unified history for local drops, remote content installs, and repairs."
                eventsModel: root.appViewModel.eventCenter
                selectedContext: root.appViewModel.selectedEventContext
                selectedEventId: root.appViewModel.selectedEventId
                statusFilter: root.appViewModel.installLogFilter
                sourceFilter: root.appViewModel.installLogSourceFilter
                typeFilter: root.appViewModel.eventCenterTypeFilter
                onEventActivated: function(eventId) { root.appViewModel.selectEvent(eventId) }
                onStatusFilterRequested: function(value) { root.appViewModel.setInstallLogFilter(value) }
                onSourceFilterRequested: function(value) { root.appViewModel.setInstallLogSourceFilter(value) }
                onTypeFilterRequested: function(value) { root.appViewModel.setEventCenterTypeFilter(value) }
                onOpenContextRequested: function() { root.appViewModel.navigateToEventContext() }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                DawnCard {
                    Layout.preferredWidth: 420
                    Layout.fillHeight: true
                    title: "Instance List"
                    subtitle: "Click a card to change the active workbench."

                    Component {
                        id: comOpenInstanceAction
                        Item {
                            FluButton {
                                anchors.centerIn: parent
                                text: "Open"
                                onClicked: {
                                    if (options && options.instanceId) {
                                        appViewModel.setActiveInstance(options.instanceId)
                                    }
                                }
                            }
                        }
                    }

                    FluTableView {
                        id: instanceTable
                        anchors.fill: parent
                        columnSource: [
                            { "title": "Active", "dataIndex": "selected", "width": 60 },
                            { "title": "Name", "dataIndex": "name", "width": 150 },
                            { "title": "MC", "dataIndex": "mcVersion", "width": 80 },
                            { "title": "Loader", "dataIndex": "loader", "width": 90 },
                            { "title": "Health", "dataIndex": "health", "width": 120 },
                            { "title": "Resources", "dataIndex": "resourceCount", "width": 90 },
                            { "title": "Java", "dataIndex": "javaProfileId", "width": 120 },
                            { "title": "Action", "dataIndex": "openAction", "width": 80 }
                        ]
                        dataSource: root.instanceTableRows()
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Instance Workbench"
                    subtitle: appViewModel.activeInstanceWorkbench.instanceName.length > 0 ? appViewModel.activeInstanceWorkbench.instanceName : "Overview, mods, packs, logs, runtime, and advanced settings."

                    FluPivot {
                        anchors.fill: parent
                        currentIndex: root.tabIndex(appViewModel.activeInstanceWorkbench.selectedTabId)
                        onCurrentIndexChanged: {
                            var tabs = appViewModel.instanceWorkbenchTabs || []
                            if (currentIndex >= 0 && currentIndex < tabs.length) {
                                appViewModel.setActiveInstanceTab(tabs[currentIndex].id)
                            }
                        }

                        Repeater {
                            model: appViewModel.instanceWorkbenchTabs

                            delegate: FluPivotItem {
                                title: modelData.title
                                contentItem: Component {
                                    DawnCard {
                                        title: modelData.title
                                        subtitle: modelData.summary

                                        Column {
                                            anchors.fill: parent
                                            spacing: 10

                                            FluText {
                                                text: "Instance: " + (appViewModel.activeInstanceWorkbench.instanceName.length > 0 ? appViewModel.activeInstanceWorkbench.instanceName : "None")
                                                color: "#f5f8fb"
                                                font.pixelSize: 18
                                                font.bold: true
                                            }

                                            FluText {
                                                text: modelData.expert ? "Expert panel: advanced overrides are collapsed by default." : "Standard panel: core actions are kept in the foreground."
                                                color: "#dce5f0"
                                                font.pixelSize: 13
                                                wrapMode: Text.WordWrap
                                            }

                                            FluTableView {
                                                id: assetTable
                                                visible: modelData.id === "mods" || modelData.id === "resourcepacks" || modelData.id === "shaderpacks" || modelData.id === "worlds" || modelData.id === "logs"
                                                width: parent.width
                                                height: 260
                                                columnSource: root.workbenchColumns(modelData.id)
                                                dataSource: root.workbenchRows(modelData.id, assetTable)
                                            }

                                            RowLayout {
                                                visible: root.isRemovableTab(modelData.id)
                                                width: parent.width
                                                spacing: 10

                                                FluButton {
                                                    visible: root.isTogglableTab(modelData.id)
                                                    text: "Enable All"
                                                    onClicked: {
                                                        var count = root.appViewModel.setAllAssetsEnabled(modelData.id, true)
                                                        var win = root.Window.window
                                                        if (win && win.showSuccess) {
                                                            win.showSuccess("Bulk enable complete", 2200, "Updated: " + count)
                                                        }
                                                    }
                                                }

                                                FluButton {
                                                    visible: root.isTogglableTab(modelData.id)
                                                    text: "Disable All"
                                                    onClicked: {
                                                        var count = root.appViewModel.setAllAssetsEnabled(modelData.id, false)
                                                        var win = root.Window.window
                                                        if (win && win.showSuccess) {
                                                            win.showSuccess("Bulk disable complete", 2200, "Updated: " + count)
                                                        }
                                                    }
                                                }

                                                FluButton {
                                                    visible: root.isTogglableTab(modelData.id)
                                                    text: "Delete Disabled"
                                                    onClicked: {
                                                        var count = root.appViewModel.removeDisabledAssets(modelData.id)
                                                        var win = root.Window.window
                                                        if (win && win.showSuccess) {
                                                            win.showSuccess("Disabled assets removed", 2200, "Removed: " + count)
                                                        }
                                                    }
                                                }

                                                FluFilledButton {
                                                    visible: root.isRemovableTab(modelData.id)
                                                    text: "Clear All"
                                                    onClicked: {
                                                        var count = root.appViewModel.removeAllAssets(modelData.id)
                                                        var win = root.Window.window
                                                        if (win && win.showSuccess) {
                                                            win.showSuccess("Assets cleared", 2200, "Removed: " + count)
                                                        }
                                                    }
                                                }
                                            }

                                            FluFrame {
                                                visible: modelData.id === "overview"
                                                width: parent.width
                                                height: 210
                                                radius: 12
                                                color: Qt.rgba(1, 1, 1, 0.03)
                                                border.color: Qt.rgba(1, 1, 1, 0.08)

                                                Column {
                                                    anchors.fill: parent
                                                    anchors.margins: 12
                                                    spacing: 8
                                                    FluText {
                                                        text: root.activePreflight().ready ? "Launch readiness: ready" : "Launch readiness: attention required"
                                                        color: root.activePreflight().ready ? "#9ce3b6" : "#f2c5ba"
                                                        font.pixelSize: 13
                                                        font.bold: true
                                                    }
                                                    FluText {
                                                        text: "Mods: " + root.assetList("mods").length
                                                              + "  |  Resourcepacks: " + root.assetList("resourcepacks").length
                                                              + "  |  Shaderpacks: " + root.assetList("shaderpacks").length
                                                              + "  |  Worlds: " + root.assetList("worlds").length
                                                        color: "#dce5f0"
                                                        font.pixelSize: 12
                                                    }
                                                    FluText {
                                                        text: root.workbenchTextFor(modelData.id)
                                                        color: "#92a3ba"
                                                        font.pixelSize: 12
                                                        wrapMode: Text.WordWrap
                                                    }
                                                }
                                            }

                                            FluFrame {
                                                visible: modelData.id === "runtime"
                                                width: parent.width
                                                height: 220
                                                radius: 12
                                                color: Qt.rgba(1, 1, 1, 0.03)
                                                border.color: Qt.rgba(1, 1, 1, 0.08)

                                                Column {
                                                    anchors.fill: parent
                                                    anchors.margins: 12
                                                    spacing: 6
                                                    FluText { text: "Java Profile: " + (appViewModel.activeInstanceAssets.runtime.javaProfileId || ""); color: "#dce5f0"; font.pixelSize: 12 }
                                                    FluText { text: "Memory Profile: " + (appViewModel.activeInstanceAssets.runtime.memoryProfile || ""); color: "#dce5f0"; font.pixelSize: 12 }
                                                    FluText { text: "Minecraft: " + (appViewModel.activeInstanceAssets.runtime.mcVersion || ""); color: "#dce5f0"; font.pixelSize: 12 }
                                                    FluText { text: "Loader: " + (appViewModel.activeInstanceAssets.runtime.loader || "") + " " + (appViewModel.activeInstanceAssets.runtime.loaderVersion || ""); color: "#dce5f0"; font.pixelSize: 12 }
                                                    FluText { text: "Java Strategy: " + appViewModel.javaStrategy; color: "#8ea0b7"; font.pixelSize: 12 }
                                                    FluText { text: "Game Dir: " + (appViewModel.activeInstanceAssets.runtime.gameDir || ""); color: "#8ea0b7"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                                }
                                            }

                                            FluFrame {
                                                visible: modelData.id === "advanced"
                                                width: parent.width
                                                height: 220
                                                radius: 12
                                                color: Qt.rgba(1, 1, 1, 0.03)
                                                border.color: Qt.rgba(1, 1, 1, 0.08)

                                                Column {
                                                    anchors.fill: parent
                                                    anchors.margins: 12
                                                    spacing: 6
                                                    FluText { text: "UI Mode: " + appViewModel.uiMode; color: "#dce5f0"; font.pixelSize: 12 }
                                                    FluText { text: "Backup Strategy: " + appViewModel.backupStrategy; color: "#dce5f0"; font.pixelSize: 12 }
                                                    FluText { text: "Scheduled Backup: " + (appViewModel.backupScheduleDate.length > 0 ? appViewModel.backupScheduleDate : "not-set") + " " + (appViewModel.backupScheduleTime.length > 0 ? appViewModel.backupScheduleTime : "not-set"); color: "#dce5f0"; font.pixelSize: 12 }
                                                    FluText { text: "Low Disk Threshold: " + appViewModel.lowDiskThresholdGb + " GB"; color: "#8ea0b7"; font.pixelSize: 12 }
                                                    FluText { text: root.workbenchTextFor(modelData.id); color: "#8ea0b7"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                                }
                                            }

                                            FluText {
                                                visible: modelData.id === "mods" || modelData.id === "resourcepacks" || modelData.id === "shaderpacks" || modelData.id === "worlds" || modelData.id === "logs"
                                                text: root.workbenchItemCount(modelData.id) > 0 ? ("Items: " + root.workbenchItemCount(modelData.id)) : "No items found in the instance directory."
                                                color: "#8ea0b7"
                                                font.pixelSize: 12
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function workbenchTextFor(tabId) {
        switch (tabId) {
        case "overview":
            return "Launch readiness, resource counts, and health are summarized here."
        case "mods":
            return "Mod enable/disable, dependency resolution, and update rollups belong here."
        case "resourcepacks":
            return "Resource pack ordering and pack-specific compatibility live here."
        case "shaderpacks":
            return "Shader installation and switching are isolated from core instance settings."
        case "worlds":
            return "World list, save exports, and restore points are grouped in this tab."
        case "logs":
            return "Launch logs and repair hints can be expanded without leaving the instance."
        case "runtime":
            return "Java runtime, memory budget, and loader bootstrap details stay visible."
        case "advanced":
            return "Expert-only flags, launch arguments, and overrides are intentionally collapsed."
        default:
            return "Workbench content is loaded from the active instance manifest."
        }
    }
}

