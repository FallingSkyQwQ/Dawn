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

                        Text {
                            text: "A calmer launcher shell for instance-centric Minecraft workflows."
                            color: "#eff4fa"
                            font.pixelSize: 18
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: "The current build exposes the core flow without depending on FluentUIbi or online services."
                            color: "#8ea0b7"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }

                    Column {
                        spacing: 10

                        Button {
                            text: "Create Demo Instance"
                            onClicked: appViewModel.createInstance("Dawn Sandbox", "1.20.1", "none")
                        }

                        Button {
                            text: "Queue Demo Task"
                            onClicked: appViewModel.enqueueDemoTask("Install stub content")
                        }
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

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 420
                    title: "Recent Instances"
                    subtitle: "Loaded from JSON manifests."

                    Column {
                        anchors.fill: parent
                        spacing: 12

                        Repeater {
                            model: appViewModel.instanceCards

                            delegate: Rectangle {
                                width: parent.width
                                height: 70
                                radius: 14
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.05)

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 12

                                    Rectangle {
                                        width: 42
                                        height: 42
                                        radius: 12
                                        color: "#66a3ff"
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.name.length > 0 ? modelData.name[0].toUpperCase() : "D"
                                            color: "white"
                                            font.pixelSize: 18
                                            font.bold: true
                                        }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text { text: modelData.name; color: "#f5f8fb"; font.pixelSize: 16; font.bold: true }
                                        Text { text: modelData.mcVersion + "  |  " + modelData.loader + "  |  " + modelData.health; color: "#92a3ba"; font.pixelSize: 12 }
                                    }

                                    Text {
                                        text: modelData.resourceCount + " resources"
                                        color: "#bfd0e7"
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }

                        Item {
                            visible: appViewModel.instanceCount === 0
                            height: 120
                            width: parent.width

                            Column {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: "No instances yet."
                                    color: "#f5f8fb"
                                    font.pixelSize: 18
                                    font.bold: true
                                }

                                Text {
                                    text: "Use the create button above to write the first manifest."
                                    color: "#8ea0b7"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 420
                    title: "Task Queue"
                    subtitle: "Download, verify, and install tasks."

                    Column {
                        anchors.fill: parent
                        spacing: 12

                        Repeater {
                            model: appViewModel.taskCards

                            delegate: Rectangle {
                                width: parent.width
                                height: 70
                                radius: 14
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.05)

                                Column {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 4

                                    Text { text: modelData.title; color: "#f5f8fb"; font.pixelSize: 15; font.bold: true }
                                    Text { text: modelData.status + "  |  " + modelData.completedSteps + "/" + modelData.stepCount + " steps"; color: "#92a3ba"; font.pixelSize: 12 }
                                }
                            }
                        }

                        Item {
                            visible: appViewModel.taskCount === 0
                            height: 120
                            width: parent.width

                            Column {
                                anchors.centerIn: parent
                                spacing: 8

                                Text {
                                    text: "Queue is idle."
                                    color: "#f5f8fb"
                                    font.pixelSize: 18
                                    font.bold: true
                                }

                                Text {
                                    text: "Use the content page to enqueue a demo install plan."
                                    color: "#8ea0b7"
                                    font.pixelSize: 12
                                }
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

                    Text {
                        text: "Windows-first shell, cross-platform fallback, optional Qt/FluentUIbi wiring, and file-backed instance state."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: "The current repository intentionally keeps online content providers and Microsoft auth as stubs."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
