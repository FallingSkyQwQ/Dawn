import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    property var appViewModel

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
                Layout.preferredHeight: 160
                title: "Logs and Repair"
                subtitle: "Preflight issues, broken dependencies, and actionable explanations."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        text: "The diagnostics model already distinguishes info, warning, and error states."
                        color: "#dce5f0"
                        font.pixelSize: 14
                    }

                    Text {
                        text: "This page will become the launch log and repair workspace once the runtime layer is wired to real downloads."
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

                Text {
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

                        Text {
                            text: "Preview diagnostics and rollback events are populated from the core install pipeline."
                            color: "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }

                        Button {
                            text: "Refresh Preview"
                            onClicked: appViewModel.refreshInstallPreview()
                        }

                        Button {
                            text: "Execute Repair"
                            enabled: appViewModel.installPreview.repairPlanAvailable
                            onClicked: appViewModel.executeRepairPlan()
                        }
                    }

                    Text {
                        text: appViewModel.installDiagnostics.length > 0 ? ("Diagnostics: " + appViewModel.installDiagnostics.length) : "No install diagnostics available yet."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }

                    Text {
                        text: "Repair status: " + appViewModel.repairExecutionStatus
                        color: "#dce5f0"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 420
                title: "Event Center"
                subtitle: "Unified install, repair, download, and diagnostic history."

                Column {
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        width: parent.width
                        spacing: 10

                        ComboBox {
                            Layout.preferredWidth: 170
                            model: [
                                { "label": "All Types", "value": "all" },
                                { "label": "Install", "value": "install" },
                                { "label": "Download", "value": "download" },
                                { "label": "Repair", "value": "repair" },
                                { "label": "Diagnostic", "value": "diagnostic" }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: appViewModel.eventCenterTypeFilter === "install" ? 1 : (appViewModel.eventCenterTypeFilter === "download" ? 2 : (appViewModel.eventCenterTypeFilter === "repair" ? 3 : (appViewModel.eventCenterTypeFilter === "diagnostic" ? 4 : 0)))
                            onActivated: appViewModel.setEventCenterTypeFilter(currentValue)
                        }

                        ComboBox {
                            Layout.preferredWidth: 150
                            model: [
                                { "label": "All", "value": "all" },
                                { "label": "Success", "value": "success" },
                                { "label": "Failure", "value": "failure" }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: appViewModel.installLogFilter === "success" ? 1 : (appViewModel.installLogFilter === "failure" ? 2 : 0)
                            onActivated: appViewModel.setInstallLogFilter(currentValue)
                        }

                        ComboBox {
                            Layout.preferredWidth: 190
                            model: [
                                { "label": "All Sources", "value": "all" },
                                { "label": "Local Drop", "value": "local_drop" },
                                { "label": "Remote Content", "value": "remote_content" },
                                { "label": "Repair", "value": "repair" },
                                { "label": "Diagnostic", "value": "diagnostic" }
                            ]
                            textRole: "label"
                            valueRole: "value"
                            currentIndex: appViewModel.installLogSourceFilter === "local_drop" ? 1 : (appViewModel.installLogSourceFilter === "remote_content" ? 2 : (appViewModel.installLogSourceFilter === "repair" ? 3 : (appViewModel.installLogSourceFilter === "diagnostic" ? 4 : 0)))
                            onActivated: appViewModel.setInstallLogSourceFilter(currentValue)
                        }

                        Item { Layout.fillWidth: true }

                        Text {
                            text: appViewModel.eventCenter.length + " entries"
                            color: "#8ea0b7"
                            font.pixelSize: 11
                        }
                    }

                    ListView {
                        width: parent.width
                        height: 210
                        clip: true
                        spacing: 8
                        model: appViewModel.eventCenter

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 74
                            radius: 12
                            color: modelData.selected ? Qt.rgba(0.24, 0.35, 0.52, 0.96) : (modelData.success ? Qt.rgba(0.14, 0.24, 0.18, 0.95) : Qt.rgba(0.28, 0.17, 0.16, 0.95))
                            border.color: modelData.selected ? Qt.rgba(0.48, 0.64, 0.98, 0.55) : Qt.rgba(1, 1, 1, 0.05)

                            MouseArea {
                                anchors.fill: parent
                                onClicked: appViewModel.selectEvent(modelData.eventId)
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

                    Rectangle {
                        width: parent.width
                        height: 92
                        radius: 12
                        color: Qt.rgba(1, 1, 1, 0.03)
                        border.color: Qt.rgba(1, 1, 1, 0.05)

                        Column {
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            Text {
                                text: ((appViewModel.selectedEventContext.eventId || "").length > 0) ? ("Selected: " + appViewModel.selectedEventContext.eventType + " -> " + appViewModel.selectedEventContext.pageHint) : "Select an event to preview context."
                                color: "#f5f8fb"
                                font.pixelSize: 13
                                font.bold: true
                            }

                            Text {
                                text: appViewModel.selectedEventContext.summary || "No event selected."
                                color: "#dce5f0"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: ((appViewModel.selectedEventContext.instanceId || "").length > 0) ? ("Instance: " + appViewModel.selectedEventContext.instanceId) : (((appViewModel.selectedEventContext.projectId || "").length > 0) ? ("Project: " + appViewModel.selectedEventContext.projectId + "  |  Version: " + appViewModel.selectedEventContext.versionId) : "No target context available.")
                                color: "#8ea0b7"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                title: "Primary Preflight"
                subtitle: appViewModel.primaryInstanceId.length > 0 ? "Instance: " + appViewModel.primaryInstanceId : "No instance selected"

                Column {
                    anchors.fill: parent
                    spacing: 12

                    Rectangle {
                        width: parent.width
                        height: 86
                        radius: 14
                        color: appViewModel.primaryPreflight.ready ? Qt.rgba(0.14, 0.26, 0.18, 0.9) : Qt.rgba(0.3, 0.18, 0.15, 0.9)
                        border.color: Qt.rgba(1, 1, 1, 0.05)

                        Column {
                            anchors.fill: parent
                            anchors.margins: 14
                            spacing: 6
                            Text { text: appViewModel.primaryPreflight.ready ? "Ready to launch" : "Preflight requires attention"; color: "#f5f8fb"; font.pixelSize: 16; font.bold: true }
                            Text { text: appViewModel.primaryPreflight.ready ? "No blocking issues were found." : "Inspect the issue list below."; color: "#dce5f0"; font.pixelSize: 12 }
                        }
                    }

                    Column {
                        spacing: 10
                        Repeater {
                            model: appViewModel.primaryPreflight.issues

                            delegate: Rectangle {
                                width: parent.width
                                height: 80
                                radius: 14
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.05)

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 4
                                    Text { text: modelData.severity.toUpperCase() + "  |  " + modelData.code; color: "#f5f8fb"; font.pixelSize: 14; font.bold: true }
                                    Text { text: modelData.message; color: "#dce5f0"; font.pixelSize: 12 }
                                    Text { text: modelData.suggestion; color: "#8ea0b7"; font.pixelSize: 12 }
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

                Column {
                    anchors.fill: parent
                    spacing: 10

                    Repeater {
                        model: appViewModel.installDiagnostics

                        delegate: Rectangle {
                            width: parent.width
                            height: 82
                            radius: 14
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.05)

                            Column {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 4
                                Text { text: modelData.severity.toUpperCase() + "  |  " + modelData.code; color: "#f5f8fb"; font.pixelSize: 14; font.bold: true }
                                Text { text: modelData.message; color: "#dce5f0"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                Text { text: modelData.suggestion; color: "#8ea0b7"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                title: "Rollback Events"
                subtitle: "Structured cleanup steps emitted when install checks fail."

                Column {
                    anchors.fill: parent
                    spacing: 10

                    Repeater {
                        model: appViewModel.rollbackEvents

                        delegate: Rectangle {
                            width: parent.width
                            height: 84
                            radius: 14
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.05)

                            Column {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 4
                                Text { text: modelData.step + "  |  " + modelData.action; color: "#f5f8fb"; font.pixelSize: 14; font.bold: true }
                                Text { text: modelData.target; color: "#dce5f0"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                                Text { text: modelData.status + (modelData.message.length > 0 ? "  |  " + modelData.message : ""); color: "#8ea0b7"; font.pixelSize: 12; wrapMode: Text.WordWrap }
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 280
                title: "Repair Execution Logs"
                subtitle: "The live repair run writes human-readable progress here."

                Column {
                    anchors.fill: parent
                    spacing: 10

                    Repeater {
                        model: appViewModel.repairExecutionLogs

                        delegate: Rectangle {
                            width: parent.width
                            height: 56
                            radius: 14
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.05)

                            Text {
                                anchors.fill: parent
                                anchors.margins: 12
                                text: modelData
                                color: "#dce5f0"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Text {
                        visible: appViewModel.repairExecutionLogs.length === 0
                        text: "No repair execution has been run yet."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }
                }
            }
        }
    }
}
