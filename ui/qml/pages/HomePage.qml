import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel
    property int homeInstancePageCurrent: 1
    property int homeInstanceItemsPerPage: 8
    property int homeTaskPageCurrent: 1
    property int homeTaskItemsPerPage: 8

    function recommendationCards() {
        var cards = []
        var instances = appViewModel ? appViewModel.instanceCards : []
        var maxItems = Math.min(instances.length, 4)
        for (var i = 0; i < maxItems; ++i) {
            var item = instances[i]
            cards.push({
                "title": item.name || "Instance",
                "subtitle": "MC " + (item.mcVersion || "-") + "  |  " + (item.loader || "none"),
                "detail": "Health: " + (item.health || "Unknown") + "  |  Resources: " + (item.resourceCount || 0),
                "accent": (item.health || "").toLowerCase().indexOf("needs") >= 0 ? "#c86b5a" : "#4c8ad8"
            })
        }
        if (cards.length === 0) {
            cards.push({
                "title": "Create your first instance",
                "subtitle": "Use the action above to bootstrap a runnable profile.",
                "detail": "Dawn will keep runtime, content, and saves isolated per instance.",
                "accent": "#4c8ad8"
            })
            cards.push({
                "title": "Queue and repair visibility",
                "subtitle": "Download and repair workflows are visible from dedicated pages.",
                "detail": "Use Content Center and Logs & Repair to inspect full execution details.",
                "accent": "#5d86b5"
            })
        }
        return cards
    }

    function homeInstanceRows() {
        var rows = []
        var data = appViewModel ? appViewModel.instanceCards : []
        for (var i = 0; i < data.length; ++i) {
            var item = data[i]
            rows.push({
                "_key": item.id || ("instance-" + i),
                "name": item.name || "",
                "mcVersion": item.mcVersion || "",
                "loader": item.loader || "",
                "health": item.health || "",
                "resourceCount": item.resourceCount || 0
            })
        }
        return rows
    }

    function homeTaskRows() {
        var rows = []
        var data = appViewModel ? appViewModel.taskCards : []
        for (var i = 0; i < data.length; ++i) {
            var item = data[i]
            rows.push({
                "_key": item.id || ("task-" + i),
                "title": item.title || "",
                "status": item.status || "",
                "completedSteps": item.completedSteps || 0,
                "stepCount": item.stepCount || 0
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
        var page = pageCurrent < 1 ? 1 : pageCurrent
        var start = (page - 1) * itemsPerPage
        if (start >= rows.length) {
            return []
        }
        return rows.slice(start, Math.min(start + itemsPerPage, rows.length))
    }

    function pagedHomeInstanceRows() {
        return pagedRows(homeInstanceRows(), homeInstancePageCurrent, homeInstanceItemsPerPage)
    }

    function pagedHomeTaskRows() {
        return pagedRows(homeTaskRows(), homeTaskPageCurrent, homeTaskItemsPerPage)
    }

    function previewPanels() {
        var panels = []
        panels.push({
            "title": "Primary Instance",
            "body": appViewModel.primaryInstanceId.length > 0
                    ? ("Current primary instance: " + appViewModel.primaryInstanceId)
                    : "No primary instance yet. Create one to unlock full runtime workflow.",
            "accent": "#4c8ad8"
        })
        panels.push({
            "title": "Preflight State",
            "body": appViewModel.primaryPreflight.ready
                    ? "Preflight checks are green for the primary instance."
                    : "Preflight found issues. Open Logs & Repair for diagnostics and repair plan.",
            "accent": appViewModel.primaryPreflight.ready ? "#4baf76" : "#c86b5a"
        })
        panels.push({
            "title": "Queue Snapshot",
            "body": "Queued tasks: " + appViewModel.taskCount + ". Download, verify, and install stages are tracked in queue and event center.",
            "accent": "#5d86b5"
        })
        return panels
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
            spacing: 20

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 170
                title: "Dawn"
                subtitle: "Instance management, content workflow, launch orchestration, and diagnostics."

                RowLayout {
                    anchors.fill: parent
                    spacing: 16

                    Column {
                        Layout.fillWidth: true
                        spacing: 10

                        FluText {
                            text: "A calmer launcher shell for instance-centric Minecraft workflows."
                            color: "#eff4fa"
                            font.pixelSize: 18
                            wrapMode: Text.WordWrap
                        }

                        FluText {
                            text: "The current build exposes the full instance workflow on top of FluentUI."
                            color: "#8ea0b7"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }

                    Column {
                        spacing: 10

                        FluFilledButton {
                            text: "Create Demo Instance"
                            onClicked: appViewModel.createInstance("Dawn Sandbox", "1.20.1", "none")
                        }

                        FluButton {
                            text: "Queue Demo Task"
                            onClicked: appViewModel.enqueueDemoTask("Install curated content")
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                title: "Recommended Workspace"
                subtitle: "Fluent carousel for recent instances and guided actions."

                FluCarousel {
                    anchors.fill: parent
                    autoPlay: true
                    loopTime: 2600
                    indicatorGravity: Qt.AlignBottom | Qt.AlignHCenter
                    indicatorMarginBottom: 10
                    model: root.recommendationCards()
                    delegate: Component {
                        Item {
                            anchors.fill: parent

                            FluFrame {
                                anchors.fill: parent
                                radius: 14
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.08)

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    width: 6
                                    radius: 3
                                    color: model.accent || "#4c8ad8"
                                }

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 18
                                    anchors.leftMargin: 28
                                    spacing: 10

                                    FluText {
                                        text: model.title || ""
                                        color: "#f5f8fb"
                                        font.pixelSize: 20
                                        font.bold: true
                                        wrapMode: Text.WordWrap
                                    }

                                    FluText {
                                        text: model.subtitle || ""
                                        color: "#dce5f0"
                                        font.pixelSize: 13
                                        wrapMode: Text.WordWrap
                                    }

                                    FluText {
                                        text: model.detail || ""
                                        color: "#8ea0b7"
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 220
                title: "Workspace FlipView"
                subtitle: "Quick vertical preview for instance, preflight, and queue state."

                FluFlipView {
                    anchors.fill: parent
                    vertical: true

                    Repeater {
                        model: root.previewPanels()

                        delegate: FluFrame {
                            anchors.fill: parent
                            radius: 12
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.08)

                            Rectangle {
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                width: 6
                                radius: 3
                                color: modelData.accent
                            }

                            Column {
                                anchors.fill: parent
                                anchors.margins: 14
                                anchors.leftMargin: 24
                                spacing: 8

                                FluText {
                                    text: modelData.title
                                    color: "#f5f8fb"
                                    font.pixelSize: 16
                                    font.bold: true
                                }

                                FluText {
                                    text: modelData.body
                                    color: "#dce5f0"
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                visible: appViewModel.firstLaunchVisible
                title: "First Launch Wizard"
                subtitle: "Complete the initial setup to hide this card."

                RowLayout {
                    anchors.fill: parent
                    spacing: 16

                    Column {
                        Layout.fillWidth: true
                        spacing: 6

                        FluText {
                            text: "Dawn is still in its initial setup flow."
                            color: "#f5f8fb"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        FluText {
                            text: "Finish the wizard once you have verified the default data root and launcher settings."
                            color: "#c5d0df"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    FluFilledButton {
                        text: "Complete First Launch"
                        onClicked: appViewModel.completeFirstLaunch()
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                DawnMetricTile {
                    Layout.fillWidth: true
                    title: "Instances"
                    value: appViewModel.instanceCount
                    hint: "Stored instance manifests in the local data root."
                }
                DawnMetricTile {
                    Layout.fillWidth: true
                    title: "Queue"
                    value: appViewModel.taskCount
                    hint: "In-memory task queue for install and repair flows."
                }
                DawnMetricTile {
                    Layout.fillWidth: true
                    title: "Primary Instance"
                    value: appViewModel.primaryInstanceId.length > 0 ? appViewModel.primaryInstanceId : "-"
                    hint: "The first discovered instance is used for dashboard summaries."
                }
                DawnMetricTile {
                    Layout.fillWidth: true
                    title: "Preflight"
                    value: appViewModel.primaryPreflight.ready ? "Ready" : "Check"
                    hint: appViewModel.primaryPreflight.ready ? "No blocking issues were detected." : "Open diagnostics to inspect warnings or errors."
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 132
                visible: appViewModel.lowDiskWarning.length > 0
                title: "Low Disk Warning"
                subtitle: "The current data root needs attention."

                FluFrame {
                    anchors.fill: parent
                    radius: 10
                    border.color: Qt.rgba(1, 1, 1, 0.12)
                    color: Qt.rgba(0.33, 0.18, 0.16, 0.45)

                    Column {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 8

                        FluText {
                            text: appViewModel.lowDiskWarning
                            color: "#f2c5ba"
                            font.pixelSize: 14
                            wrapMode: Text.WordWrap
                        }

                        FluText {
                            text: "Probe path: " + appViewModel.diskSpaceStatus.path
                            color: "#c5d0df"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 210
                title: "Storage Status"
                subtitle: "Disk availability and cache maintenance summary."

                Column {
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        width: parent.width
                        spacing: 14

                        Column {
                            Layout.fillWidth: true
                            spacing: 4
                            FluText { text: "Probe path"; color: "#8ea0b7"; font.pixelSize: 11 }
                            FluText { text: appViewModel.diskSpaceStatus.path; color: "#f5f8fb"; font.pixelSize: 13; wrapMode: Text.WordWrap }
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 4
                            FluText { text: "Available"; color: "#8ea0b7"; font.pixelSize: 11 }
                            FluText { text: appViewModel.diskSpaceStatus.availableDisplay; color: "#f5f8fb"; font.pixelSize: 13 }
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 4
                            FluText { text: "Threshold"; color: "#8ea0b7"; font.pixelSize: 11 }
                            FluText { text: appViewModel.diskSpaceStatus.thresholdDisplay; color: "#f5f8fb"; font.pixelSize: 13 }
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 4
                            FluText { text: "State"; color: "#8ea0b7"; font.pixelSize: 11 }
                            FluText { text: appViewModel.diskSpaceStatus.statusLabel; color: appViewModel.diskSpaceStatus.low ? "#f2c5ba" : "#9ce3b6"; font.pixelSize: 13; font.bold: true }
                        }
                    }

                    FluDivider {
                        width: parent.width
                    }

                    Column {
                        spacing: 4
                        FluText {
                            text: "Last cache cleanup"
                            color: "#8ea0b7"
                            font.pixelSize: 11
                        }
                        FluText {
                            text: appViewModel.cacheCleanupSummary.statusLabel + "  |  before " + appViewModel.cacheCleanupSummary.bytesBeforeDisplay + "  ->  after " + appViewModel.cacheCleanupSummary.bytesAfterDisplay + "  |  freed " + appViewModel.cacheCleanupSummary.bytesFreedDisplay
                            color: "#f5f8fb"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                        FluText {
                            text: appViewModel.cacheCleanupSummary.message.length > 0 ? appViewModel.cacheCleanupSummary.message : "No cleanup has been executed in this session."
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 460
                    title: "Recent Instances"
                    subtitle: "Loaded from JSON manifests."

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        FluTableView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            columnSource: [
                                { "title": "Name", "dataIndex": "name", "width": 180 },
                                { "title": "MC", "dataIndex": "mcVersion", "width": 90 },
                                { "title": "Loader", "dataIndex": "loader", "width": 100 },
                                { "title": "Health", "dataIndex": "health", "width": 140 },
                                { "title": "Resources", "dataIndex": "resourceCount", "width": 110 }
                            ]
                            dataSource: root.pagedHomeInstanceRows()
                        }

                        FluPagination {
                            Layout.alignment: Qt.AlignHCenter
                            pageCurrent: root.homeInstancePageCurrent
                            pageButtonCount: 5
                            itemCount: root.appViewModel ? root.appViewModel.instanceCards.length : 0
                            __itemPerPage: root.homeInstanceItemsPerPage
                            onRequestPage: function(page) {
                                root.homeInstancePageCurrent = page
                            }
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 460
                    title: "Task Queue"
                    subtitle: "Download, verify, and install tasks."

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: 10

                        FluTableView {
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            columnSource: [
                                { "title": "Title", "dataIndex": "title", "width": 320 },
                                { "title": "Status", "dataIndex": "status", "width": 130 },
                                { "title": "Done", "dataIndex": "completedSteps", "width": 90 },
                                { "title": "Steps", "dataIndex": "stepCount", "width": 90 }
                            ]
                            dataSource: root.pagedHomeTaskRows()
                        }

                        FluPagination {
                            Layout.alignment: Qt.AlignHCenter
                            pageCurrent: root.homeTaskPageCurrent
                            pageButtonCount: 5
                            itemCount: root.appViewModel ? root.appViewModel.taskCards.length : 0
                            __itemPerPage: root.homeTaskItemsPerPage
                            onRequestPage: function(page) {
                                root.homeTaskPageCurrent = page
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                title: "System Snapshot"
                subtitle: "The shell stays readable even when advanced integrations are missing."

                Column {
                    anchors.fill: parent
                    spacing: 10

                    FluText {
                        text: "Cross-platform FluentUI shell with file-backed instance state and unified task orchestration."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }

                    FluText {
                        text: "Online content providers and Microsoft account flows are integrated through real service pipelines."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}

