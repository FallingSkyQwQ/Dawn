import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel
    property int repairLogPageCurrent: 1
    property int repairLogItemsPerPage: 12

    function statusProgressValue(status) {
        var normalized = (status || "").toLowerCase()
        if (normalized === "succeeded" || normalized === "done" || normalized === "completed") {
            return 1
        }
        if (normalized === "failed" || normalized === "error" || normalized === "blocked") {
            return 1
        }
        if (normalized === "running" || normalized === "in_progress" || normalized === "pending") {
            return 0.35
        }
        return 0
    }

    function repairPlanProgressValue() {
        var steps = appViewModel ? appViewModel.installPreview.repairPlan.steps : []
        if (!steps || steps.length === 0) {
            return 0
        }
        var score = 0
        for (var i = 0; i < steps.length; ++i) {
            score += statusProgressValue(steps[i].status)
        }
        var ratio = score / steps.length
        if (ratio < 0) {
            return 0
        }
        if (ratio > 1) {
            return 1
        }
        return ratio
    }

    function repairPlanProgressText() {
        return Math.round(repairPlanProgressValue() * 100) + "%"
    }

    function diagnosticsRows() {
        var rows = []
        var data = appViewModel ? appViewModel.installDiagnostics : []
        for (var i = 0; i < data.length; ++i) {
            var item = data[i]
            rows.push({
                "_key": (item.code || "diag") + "-" + i,
                "severity": item.severity || "",
                "code": item.code || "",
                "message": item.message || "",
                "suggestion": item.suggestion || ""
            })
        }
        return rows
    }

    function rollbackRows() {
        var rows = []
        var data = appViewModel ? appViewModel.rollbackEvents : []
        for (var i = 0; i < data.length; ++i) {
            var item = data[i]
            rows.push({
                "_key": (item.step || "rollback") + "-" + i,
                "step": item.step || "",
                "action": item.action || "",
                "target": item.target || "",
                "status": item.status || "",
                "message": item.message || ""
            })
        }
        return rows
    }

    function repairLogRows() {
        var rows = []
        var data = appViewModel ? appViewModel.repairExecutionLogs : []
        for (var i = 0; i < data.length; ++i) {
            rows.push({
                "_key": "repair-log-" + i,
                "index": i + 1,
                "line": data[i]
            })
        }
        return rows
    }

    function pagedRows(rows, pageCurrent, itemsPerPage) {
        if (!rows || rows.length === 0) {
            return []
        }
        if (itemsPerPage <= 0) {
            return rows
        }
        var page = pageCurrent
        if (page < 1) {
            page = 1
        }
        var start = (page - 1) * itemsPerPage
        if (start >= rows.length) {
            return []
        }
        var end = Math.min(start + itemsPerPage, rows.length)
        return rows.slice(start, end)
    }

    function repairLogPageCount() {
        var data = appViewModel ? appViewModel.repairExecutionLogs : []
        return data.length > 0 ? Math.ceil(data.length / repairLogItemsPerPage) : 0
    }

    function normalizePage(page, count) {
        if (count <= 0) {
            return 1
        }
        if (page < 1) {
            return 1
        }
        if (page > count) {
            return count
        }
        return page
    }

    function pagedRepairLogRows() {
        return pagedRows(repairLogRows(), repairLogPageCurrent, repairLogItemsPerPage)
    }

    Connections {
        target: root.appViewModel
        function onDataChanged() {
            root.repairLogPageCurrent = root.normalizePage(root.repairLogPageCurrent, root.repairLogPageCount())
        }
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: layout.implicitHeight
        clip: true
        ScrollBar.vertical: FluScrollBar {}

        ColumnLayout {
            id: layout
            width: parent.width
            spacing: 18

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 160
                title: "Logs and Repair"
                subtitle: "Preflight issues, broken dependencies, and actionable explanations."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    FluText {
                        text: "The diagnostics model already distinguishes info, warning, and error states."
                        color: "#dce5f0"
                        font.pixelSize: 14
                    }

                    FluText {
                        text: "Launch diagnostics and repair execution are driven by the same install and runtime service pipeline."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 120
                visible: appViewModel.lowDiskWarning.length > 0
                title: "Low Disk Warning"
                subtitle: "The current data root has limited free space."

                FluText {
                    anchors.fill: parent
                    text: appViewModel.lowDiskWarning
                    color: "#f2c5ba"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                title: "Install Preview"
                subtitle: appViewModel.installPreviewStatus

                Column {
                    anchors.fill: parent
                    spacing: 8

                    RowLayout {
                        width: parent.width

                        FluText {
                            text: "Preview diagnostics and rollback events are populated from the core install pipeline."
                            color: "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        FluButton {
                            text: "Refresh Preview"
                            onClicked: appViewModel.refreshInstallPreview()
                        }

                        FluFilledButton {
                            text: "Execute Repair"
                            enabled: appViewModel.installPreview.repairPlanAvailable
                            onClicked: appViewModel.executeRepairPlan()
                        }
                    }

                    FluText {
                        text: appViewModel.installDiagnostics.length > 0 ? ("Diagnostics: " + appViewModel.installDiagnostics.length) : "No install diagnostics available yet."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }

                    FluText {
                        text: "Repair status: " + appViewModel.repairExecutionStatus
                        color: "#dce5f0"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        width: parent.width
                        spacing: 12

                        FluProgressRing {
                            Layout.preferredWidth: 48
                            Layout.preferredHeight: 48
                            indeterminate: false
                            progressVisible: true
                            value: root.repairPlanProgressValue()
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 6

                            FluText {
                                text: "Repair plan progress: " + root.repairPlanProgressText()
                                color: "#dce5f0"
                                font.pixelSize: 12
                            }

                            FluProgressBar {
                                width: parent.width
                                indeterminate: false
                                progressVisible: true
                                value: root.repairPlanProgressValue()
                            }
                        }
                    }
                }
            }

            EventCenterPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                title: "Event Center"
                subtitle: "Unified install, repair, download, and diagnostic history."
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
            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                title: "Primary Preflight"
                subtitle: appViewModel.primaryInstanceId.length > 0 ? "Instance: " + appViewModel.primaryInstanceId : "No instance selected"

                Column {
                    anchors.fill: parent
                    spacing: 12

                    FluFrame {
                        width: parent.width
                        height: 86
                        radius: 14
                        color: appViewModel.primaryPreflight.ready ? Qt.rgba(0.14, 0.26, 0.18, 0.9) : Qt.rgba(0.3, 0.18, 0.15, 0.9)
                        border.color: Qt.rgba(1, 1, 1, 0.05)

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 6
                            FluText { text: appViewModel.primaryPreflight.ready ? "Ready to launch" : "Preflight requires attention"; color: "#f5f8fb"; font.pixelSize: 16; font.bold: true }
                            FluText { text: appViewModel.primaryPreflight.ready ? "No blocking issues were found." : "Inspect the issue list below."; color: "#dce5f0"; font.pixelSize: 12 }
                        }
                    }

                    Column {
                        spacing: 10
                        Repeater {
                            model: appViewModel.primaryPreflight.issues

                            delegate: FluFrame {
                                width: parent.width
                                height: 80
                                radius: 14
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.05)

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 4
                                    FluText { text: modelData.severity.toUpperCase() + "  |  " + modelData.code; color: "#f5f8fb"; font.pixelSize: 14; font.bold: true }
                                    FluText { text: modelData.message; color: "#dce5f0"; font.pixelSize: 12 }
                                    FluText { text: modelData.suggestion; color: "#8ea0b7"; font.pixelSize: 12 }
                                }
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                title: "Install Diagnostics"
                subtitle: "Structured dependency checks and conflict reasons."

                FluTableView {
                    anchors.fill: parent
                    columnSource: [
                        { "title": "Severity", "dataIndex": "severity", "width": 90 },
                        { "title": "Code", "dataIndex": "code", "width": 150 },
                        { "title": "Message", "dataIndex": "message", "width": 360 },
                        { "title": "Suggestion", "dataIndex": "suggestion", "width": 300 }
                    ]
                    dataSource: root.diagnosticsRows()
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                title: "Rollback Events"
                subtitle: "Structured cleanup steps emitted when install checks fail."

                FluTableView {
                    anchors.fill: parent
                    columnSource: [
                        { "title": "Step", "dataIndex": "step", "width": 140 },
                        { "title": "Action", "dataIndex": "action", "width": 160 },
                        { "title": "Target", "dataIndex": "target", "width": 300 },
                        { "title": "Status", "dataIndex": "status", "width": 120 },
                        { "title": "Message", "dataIndex": "message", "width": 220 }
                    ]
                    dataSource: root.rollbackRows()
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 330
                title: "Repair Execution Logs"
                subtitle: "The live repair run writes human-readable progress here."

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    FluTableView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columnSource: [
                            { "title": "#", "dataIndex": "index", "width": 60 },
                            { "title": "Log", "dataIndex": "line", "width": 900 }
                        ]
                        dataSource: root.pagedRepairLogRows()
                    }

                    FluPagination {
                        Layout.alignment: Qt.AlignHCenter
                        pageCurrent: root.repairLogPageCurrent
                        pageButtonCount: 5
                        itemCount: root.appViewModel ? root.appViewModel.repairExecutionLogs.length : 0
                        __itemPerPage: root.repairLogItemsPerPage
                        onRequestPage: function(page) {
                            root.repairLogPageCurrent = page
                        }
                    }
                }
            }
        }
    }
}
