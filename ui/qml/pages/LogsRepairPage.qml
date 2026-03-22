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
