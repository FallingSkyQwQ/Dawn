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
                Layout.preferredHeight: 140
                title: "Download Queue"
                subtitle: "A unified queue for downloads, verification, extraction, and install steps."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        text: "Task states are managed by the core queue model and surfaced to QML as cards."
                        color: "#dce5f0"
                        font.pixelSize: 14
                    }

                    Text {
                        text: "The actual network and decompression work is intentionally stubbed in this first pass."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 12

                Repeater {
                    model: appViewModel.taskCards

                    delegate: DawnCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 140
                        title: modelData.title
                        subtitle: modelData.status

                        Column {
                            anchors.fill: parent
                            spacing: 8

                            Text {
                                text: "Steps: " + modelData.completedSteps + " / " + modelData.stepCount
                                color: "#f5f8fb"
                                font.pixelSize: 14
                            }

                            Rectangle {
                                width: parent.width
                                height: 8
                                radius: 4
                                color: Qt.rgba(1, 1, 1, 0.08)

                                Rectangle {
                                    width: parent.width * (modelData.stepCount === 0 ? 0 : modelData.completedSteps / modelData.stepCount)
                                    height: parent.height
                                    radius: 4
                                    color: "#66a3ff"
                                }
                            }
                        }
                    }
                }

                Item {
                    visible: appViewModel.taskCount === 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: 220

                    DawnCard {
                        anchors.fill: parent
                        title: "Idle Queue"
                        subtitle: "No tasks are waiting. Queue a demo install from the content page."

                        Column {
                            anchors.centerIn: parent
                            spacing: 8

                            Text { text: "The queue is empty."; color: "#f5f8fb"; font.pixelSize: 18; font.bold: true }
                            Text { text: "Pause, resume, retry, and concurrent scheduling are reserved for the next phase."; color: "#8ea0b7"; font.pixelSize: 12 }
                        }
                    }
                }
            }
        }
    }
}
