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
        }
    }
}
