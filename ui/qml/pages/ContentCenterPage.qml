pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    property var appViewModel

    function findIndexById(list, id, roleName) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i] && list[i][roleName] === id) {
                return i
            }
        }
        return list.length > 0 ? 0 : -1
    }

    function searchNow() {
        root.appViewModel.searchContent(searchField.text, projectTypeBox.currentValue)
    }

    function flattenTree(node, depth, rows) {
        if (!node || !node.id) {
            return
        }
        rows.push({
            "depth": depth,
            "id": node.id,
            "versionId": node.versionId,
            "status": node.status,
            "requirement": node.requirement,
            "message": node.message
        })
        if (node.children) {
            for (var i = 0; i < node.children.length; ++i) {
                flattenTree(node.children[i], depth + 1, rows)
            }
        }
    }

    function dependencyRows() {
        var rows = []
        flattenTree(root.appViewModel.installPreview.dependencyTree, 0, rows)
        return rows
    }

    function dependencyStatusColor(status) {
        switch (status) {
        case "ready": return "#4bd18f"
        case "installed": return "#4bd18f"
        case "missing": return "#ffb74d"
        case "conflict": return "#ff6b6b"
        case "blocked": return "#ff6b6b"
        case "optional": return "#8ea0b7"
        case "embedded": return "#7aa2ff"
        default: return "#dce5f0"
        }
    }

    function rowsForInstances() {
        return root.appViewModel.instanceCards
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 18

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                title: "Content Center"
                subtitle: "Search content, choose a target instance, inspect dependencies, and preview a repair plan."

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        TextField {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: "Search mods, modpacks, resource packs, shaders"
                            onAccepted: root.searchNow()
                        }

                        ComboBox {
                            id: projectTypeBox
                            Layout.preferredWidth: 180
                            textRole: "label"
                            valueRole: "value"
                            model: [
                                { "label": "Mod", "value": "mod" },
                                { "label": "Modpack", "value": "modpack" },
                                { "label": "Resourcepack", "value": "resourcepack" },
                                { "label": "Shader", "value": "shader" }
                            ]
                            currentIndex: 0
                        }

                        Button {
                            text: "Search"
                            onClicked: root.searchNow()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        ComboBox {
                            id: instanceBox
                            Layout.fillWidth: true
                            textRole: "name"
                            valueRole: "id"
                            model: root.rowsForInstances()
                            currentIndex: root.findIndexById(root.rowsForInstances(), root.appViewModel.selectedTargetInstanceId, "id")
                            onActivated: root.appViewModel.selectTargetInstance(currentValue)
                        }

                        Button {
                            text: "Refresh Preview"
                            onClicked: root.appViewModel.refreshInstallPreview()
                        }
                    }

                    Text {
                        text: root.appViewModel.installPreviewStatus
                        color: root.appViewModel.installPreview.blocked ? "#ffb74d" : "#4bd18f"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        Button {
                            text: "Install Selected"
                            enabled: !root.appViewModel.installPreview.blocked && root.appViewModel.selectedContentProjectId.length > 0 && root.appViewModel.selectedContentVersionId.length > 0
                            onClicked: root.appViewModel.installSelectedContent()
                        }

                        Text {
                            Layout.fillWidth: true
                            text: root.appViewModel.contentInstallStatus
                            color: "#dce5f0"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.preferredWidth: parent.width * 0.38
                    Layout.fillHeight: true
                    spacing: 16

                    DawnCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 340
                        title: "Search Results"
                        subtitle: root.appViewModel.contentSearchResults.length > 0 ? "Select a result to load versions and preview." : "Run a search to populate results."

                        Column {
                            anchors.fill: parent
                            spacing: 10

                            Repeater {
                                model: root.appViewModel.contentSearchResults

                                delegate: Rectangle {
                                    width: parent.width
                                    height: 96
                                    radius: 14
                                    color: modelData.selected ? Qt.rgba(0.23, 0.34, 0.5, 0.95) : Qt.rgba(1, 1, 1, 0.03)
                                    border.color: modelData.selected ? Qt.rgba(0.48, 0.64, 0.98, 0.45) : Qt.rgba(1, 1, 1, 0.05)
                                    border.width: 1

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: root.appViewModel.selectSearchResult(modelData.projectId)
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 12

                                        Rectangle {
                                            Layout.preferredWidth: 48
                                            Layout.preferredHeight: 48
                                            radius: 14
                                            color: modelData.selected ? "#8ec5ff" : "#66a3ff"
                                            Text {
                                                anchors.centerIn: parent
                                                text: modelData.title.length > 0 ? modelData.title[0].toUpperCase() : "D"
                                                color: "white"
                                                font.pixelSize: 18
                                                font.bold: true
                                            }
                                        }

                                        Column {
                                            Layout.fillWidth: true
                                            spacing: 3
                                            Text { text: modelData.title; color: "#f5f8fb"; font.pixelSize: 15; font.bold: true }
                                            Text { text: modelData.author + "  |  " + modelData.downloads + " downloads"; color: "#9eb0c7"; font.pixelSize: 12 }
                                            Text { text: modelData.summary; color: "#8ea0b7"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                        }
                                    }
                                }
                            }

                            Item {
                                visible: root.appViewModel.contentSearchResults.length === 0
                                height: 120
                                width: parent.width
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 8
                                    Text { text: "No search results yet."; color: "#f5f8fb"; font.pixelSize: 16; font.bold: true }
                                    Text { text: "Search by name or leave the default demo query to inspect the workflow."; color: "#8ea0b7"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                }
                            }
                        }
                    }

                    DawnCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 280
                        title: "Versions"
                        subtitle: root.appViewModel.contentVersions.length > 0 ? "Pick a version for the selected project." : "No versions loaded yet."

                        Column {
                            anchors.fill: parent
                            spacing: 10

                            Repeater {
                                model: root.appViewModel.contentVersions

                                delegate: Rectangle {
                                    width: parent.width
                                    height: 78
                                    radius: 14
                                    color: modelData.selected ? Qt.rgba(0.18, 0.28, 0.42, 0.95) : Qt.rgba(1, 1, 1, 0.03)
                                    border.color: modelData.selected ? Qt.rgba(0.48, 0.64, 0.98, 0.45) : Qt.rgba(1, 1, 1, 0.05)
                                    border.width: 1

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: root.appViewModel.selectInstallVersion(modelData.versionId)
                                    }

                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 12
                                        spacing: 4
                                        Text { text: modelData.name.length > 0 ? modelData.name : modelData.versionId; color: "#f5f8fb"; font.pixelSize: 14; font.bold: true }
                                        Text { text: modelData.gameVersions.length > 0 ? modelData.gameVersions.join(", ") : "Any game version"; color: "#9eb0c7"; font.pixelSize: 12 }
                                        Text { text: modelData.loaders.length > 0 ? modelData.loaders.join(", ") : "Any loader"; color: "#8ea0b7"; font.pixelSize: 12 }
                                    }
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: parent.width * 0.62
                    Layout.fillHeight: true
                    spacing: 16

                    DawnCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 220
                        title: "Install Preview"
                        subtitle: root.appViewModel.installPreview.blocked ? "Blocked preview with actionable diagnostics." : "Preview is ready to install."

                        Column {
                            anchors.fill: parent
                            spacing: 8

                            Text {
                                text: "Project: " + (root.appViewModel.installPreview.projectId.length > 0 ? root.appViewModel.installPreview.projectId : "none")
                                color: "#dce5f0"
                                font.pixelSize: 13
                            }

                            Text {
                                text: "Version: " + (root.appViewModel.installPreview.versionId.length > 0 ? root.appViewModel.installPreview.versionId : "none")
                                color: "#dce5f0"
                                font.pixelSize: 13
                            }

                            Text {
                                text: "Target: " + (root.appViewModel.installPreview.targetInstanceId.length > 0 ? root.appViewModel.installPreview.targetInstanceId : "none")
                                color: "#dce5f0"
                                font.pixelSize: 13
                            }

                            Text {
                                text: root.appViewModel.installPreview.blocked ? "Preview blocked" : "Preview ready"
                                color: root.appViewModel.installPreview.blocked ? "#ffb74d" : "#4bd18f"
                                font.pixelSize: 16
                                font.bold: true
                            }

                            Text {
                                text: "Dependencies, version suggestions, and repair plan are derived from the current selection."
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    DawnCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 330
                        title: "Event Center"
                        subtitle: "Unified install, repair, download, and diagnostic history."

                        Column {
                            anchors.fill: parent
                            spacing: 10

                            RowLayout {
                                width: parent.width
                                spacing: 10

                                ComboBox {
                                    Layout.preferredWidth: 150
                                    textRole: "label"
                                    valueRole: "value"
                                    model: [
                                        { "label": "All", "value": "all" },
                                        { "label": "Success", "value": "success" },
                                        { "label": "Failure", "value": "failure" }
                                    ]
                                    currentIndex: root.appViewModel.installLogFilter === "success" ? 1 : (root.appViewModel.installLogFilter === "failure" ? 2 : 0)
                                    onActivated: root.appViewModel.setInstallLogFilter(currentValue)
                                }

                                ComboBox {
                                    Layout.preferredWidth: 190
                                    textRole: "label"
                                    valueRole: "value"
                                    model: [
                                        { "label": "All Sources", "value": "all" },
                                        { "label": "Local Drop", "value": "local_drop" },
                                        { "label": "Remote Content", "value": "remote_content" },
                                        { "label": "Repair", "value": "repair" },
                                        { "label": "Diagnostic", "value": "diagnostic" }
                                    ]
                                    currentIndex: root.appViewModel.installLogSourceFilter === "local_drop" ? 1 : (root.appViewModel.installLogSourceFilter === "remote_content" ? 2 : (root.appViewModel.installLogSourceFilter === "repair" ? 3 : (root.appViewModel.installLogSourceFilter === "diagnostic" ? 4 : 0)))
                                    onActivated: root.appViewModel.setInstallLogSourceFilter(currentValue)
                                }

                                ComboBox {
                                    Layout.preferredWidth: 150
                                    textRole: "label"
                                    valueRole: "value"
                                    model: [
                                        { "label": "All Types", "value": "all" },
                                        { "label": "Install", "value": "install" },
                                        { "label": "Download", "value": "download" },
                                        { "label": "Repair", "value": "repair" },
                                        { "label": "Diagnostic", "value": "diagnostic" }
                                    ]
                                    currentIndex: root.appViewModel.eventCenterTypeFilter === "install" ? 1 : (root.appViewModel.eventCenterTypeFilter === "download" ? 2 : (root.appViewModel.eventCenterTypeFilter === "repair" ? 3 : (root.appViewModel.eventCenterTypeFilter === "diagnostic" ? 4 : 0)))
                                    onActivated: root.appViewModel.setEventCenterTypeFilter(currentValue)
                                }

                                Button {
                                    text: "Open Context"
                                    enabled: (root.appViewModel.selectedEventContext.eventId || "").length > 0
                                    onClicked: root.appViewModel.navigateToEventContext()
                                }

                                Item { Layout.fillWidth: true }

                                Text {
                                    text: root.appViewModel.eventCenter.length + " entries"
                                    color: "#8ea0b7"
                                    font.pixelSize: 11
                                }
                            }

                            ListView {
                                width: parent.width
                                height: 208
                                clip: true
                                spacing: 8
                                model: root.appViewModel.eventCenter

                                delegate: Rectangle {
                                    width: ListView.view.width
                                    height: 74
                                    radius: 12
                                    color: modelData.selected ? Qt.rgba(0.24, 0.35, 0.52, 0.96) : (modelData.success ? Qt.rgba(0.14, 0.24, 0.18, 0.95) : Qt.rgba(0.28, 0.17, 0.16, 0.95))
                                    border.color: modelData.selected ? Qt.rgba(0.48, 0.64, 0.98, 0.55) : Qt.rgba(1, 1, 1, 0.05)

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: root.appViewModel.selectEvent(modelData.eventId)
                                    }

                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 3
                                        Text { text: modelData.time + "  |  " + modelData.eventType + "  |  " + modelData.sourceType + "  |  " + modelData.result; color: "#f5f8fb"; font.pixelSize: 12; font.bold: true }
                                        Text { text: "Target: " + modelData.targetInstanceId + "  |  " + modelData.summary; color: "#dce5f0"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                                    }
                                }
                            }

                            Text {
                                text: (root.appViewModel.selectedEventContext.eventId || "").length > 0 ? ("Context: " + root.appViewModel.selectedEventContext.eventType + " -> " + root.appViewModel.selectedEventContext.eventTargetPage + " | Instance " + (root.appViewModel.eventTargetInstanceId.length > 0 ? root.appViewModel.eventTargetInstanceId : "none") + " | Project " + (root.appViewModel.eventTargetProjectId.length > 0 ? root.appViewModel.eventTargetProjectId : "none")) : "Select an event to preview its target context."
                                color: "#8ea0b7"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 300
                            title: "Dependency Tree"
                            subtitle: "Structured blockers and dependencies."

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Repeater {
                                    model: root.dependencyRows()

                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 52
                                        radius: 12
                                        color: Qt.rgba(1, 1, 1, 0.03)
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 8

                                            Item { Layout.preferredWidth: modelData.depth * 18; Layout.preferredHeight: 1 }

                                            Column {
                                                Layout.fillWidth: true
                                                spacing: 2
                                                Text { text: modelData.id + (modelData.versionId.length > 0 ? " @ " + modelData.versionId : ""); color: "#f5f8fb"; font.pixelSize: 13; font.bold: true }
                                                Text { text: modelData.message; color: "#8ea0b7"; font.pixelSize: 11 }
                                            }

                                            Text {
                                                text: modelData.status
                                                color: root.dependencyStatusColor(modelData.status)
                                                font.pixelSize: 11
                                                font.bold: true
                                            }
                                        }
                                    }
                                }
                            }
                        }

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 300
                            title: "Version Suggestions"
                            subtitle: "Recommended alternatives for the target instance."

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Repeater {
                                    model: root.appViewModel.installPreview.versionSuggestions

                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 76
                                        radius: 12
                                        color: modelData.recommended ? Qt.rgba(0.16, 0.24, 0.36, 0.95) : Qt.rgba(1, 1, 1, 0.03)
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: root.appViewModel.selectInstallVersion(modelData.versionId)
                                        }

                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 3
                                            Text { text: modelData.name; color: "#f5f8fb"; font.pixelSize: 13; font.bold: true }
                                            Text { text: modelData.reason; color: "#8ea0b7"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            title: "Repair Plan"
                            subtitle: root.appViewModel.installPreview.repairPlanAvailable ? "One-click repair plan is available." : "No repair plan required."

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Repeater {
                                    model: root.appViewModel.installPreview.repairPlan.steps

                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 62
                                        radius: 12
                                        color: Qt.rgba(1, 1, 1, 0.03)
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 3
                                            Text { text: modelData.title; color: "#f5f8fb"; font.pixelSize: 13; font.bold: true }
                                            Text { text: modelData.detail.length > 0 ? modelData.detail : modelData.status; color: "#8ea0b7"; font.pixelSize: 11 }
                                        }
                                    }
                                }
                            }
                        }

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            title: "Diagnostics"
                            subtitle: "Blocking and informational messages from the preview."

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Repeater {
                                    model: root.appViewModel.installDiagnostics

                                    delegate: Rectangle {
                                        width: parent.width
                                        height: 62
                                        radius: 12
                                        color: Qt.rgba(1, 1, 1, 0.03)
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 3
                                            Text { text: modelData.code + "  |  " + modelData.severity; color: "#f5f8fb"; font.pixelSize: 12; font.bold: true }
                                            Text { text: modelData.message; color: "#8ea0b7"; font.pixelSize: 11; wrapMode: Text.WordWrap }
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
