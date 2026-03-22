import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel

    function progressValue(completedSteps, stepCount) {
        if (!stepCount || stepCount <= 0) {
            return 0
        }
        var ratio = completedSteps / stepCount
        if (ratio < 0) {
            return 0
        }
        if (ratio > 1) {
            return 1
        }
        return ratio
    }

    function queueProgressValue() {
        var cards = appViewModel ? appViewModel.taskCards : []
        if (cards.length === 0) {
            return 0
        }
        var totalDone = 0
        var totalSteps = 0
        for (var i = 0; i < cards.length; ++i) {
            totalDone += cards[i].completedSteps || 0
            totalSteps += cards[i].stepCount || 0
        }
        return progressValue(totalDone, totalSteps)
    }

    function queueProgressText() {
        var value = Math.round(queueProgressValue() * 100)
        return value + "%"
    }

    function taskRows() {
        var rows = []
        var cards = appViewModel ? appViewModel.taskCards : []
        for (var i = 0; i < cards.length; ++i) {
            var item = cards[i]
            var ratio = progressValue(item.completedSteps || 0, item.stepCount || 0)
            rows.push({
                "_key": item.id || ("task-row-" + i),
                "title": item.title || "",
                "status": item.status || "",
                "completedSteps": item.completedSteps || 0,
                "stepCount": item.stepCount || 0,
                "progress": Math.round(ratio * 100) + "%",
                "progressBar": queueTable.customItem(comTaskProgress, { "value": ratio })
            })
        }
        return rows
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
                Layout.preferredHeight: 140
                title: "Download Queue"
                subtitle: "A unified queue for downloads, verification, extraction, and install steps."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    FluText {
                        text: "Task states are managed by the core queue model and surfaced to QML as cards."
                        color: "#dce5f0"
                        font.pixelSize: 14
                    }

                    FluText {
                        text: "Network transfer, verification, and install execution are orchestrated through the task pipeline."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 150
                title: "Queue Progress"
                subtitle: appViewModel.taskCount > 0 ? "Live progress from queued task steps." : "No active tasks."

                RowLayout {
                    anchors.fill: parent
                    spacing: 16

                    FluProgressRing {
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 56
                        indeterminate: false
                        progressVisible: true
                        value: root.queueProgressValue()
                    }

                    Column {
                        Layout.fillWidth: true
                        spacing: 8

                        FluText {
                            text: "Overall completion: " + root.queueProgressText()
                            color: "#f5f8fb"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        FluProgressBar {
                            width: parent.width
                            indeterminate: false
                            progressVisible: true
                            value: root.queueProgressValue()
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                title: "Queue Table"
                subtitle: "Structured queue rows rendered with FluTableView."

                Component {
                    id: comTaskProgress
                    Item {
                        FluProgressBar {
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(parent.width - 12, 24)
                            x: 6
                            indeterminate: false
                            progressVisible: false
                            value: options && options.value !== undefined ? options.value : 0
                        }
                    }
                }

                FluTableView {
                    id: queueTable
                    anchors.fill: parent
                    columnSource: [
                        { "title": "Task", "dataIndex": "title", "width": 300 },
                        { "title": "Status", "dataIndex": "status", "width": 120 },
                        { "title": "Completed", "dataIndex": "completedSteps", "width": 90 },
                        { "title": "Steps", "dataIndex": "stepCount", "width": 80 },
                        { "title": "Progress", "dataIndex": "progress", "width": 80 },
                        { "title": "Bar", "dataIndex": "progressBar", "width": 170 }
                    ]
                    dataSource: root.taskRows()
                }
            }

            DawnCard {
                visible: appViewModel.taskCount === 0
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                title: "Idle Queue"
                subtitle: "No tasks are waiting. Queue a demo install from the content page."

                Column {
                    anchors.centerIn: parent
                    spacing: 8

                    FluText { text: "The queue is empty."; color: "#f5f8fb"; font.pixelSize: 18; font.bold: true }
                    FluText { text: "Pause, resume, retry, and concurrent scheduling are reserved for the next phase."; color: "#8ea0b7"; font.pixelSize: 12 }
                }
            }
        }
    }
}

