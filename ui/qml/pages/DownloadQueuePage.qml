import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel
    property int queuePageCurrent: 1
    property int queueItemsPerPage: 8

    // Animation durations
    readonly property int microInteractionDuration: 140  // 120-160ms
    readonly property int panelSwitchDuration: 200  // 180-220ms
    readonly property int pageTransitionDuration: 260  // 240-300ms
    readonly property int staggerDelay: 40

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

    function queuePageRows() {
        return pagedRows(taskRows(), queuePageCurrent, queueItemsPerPage)
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
                Layout.preferredHeight: 140
                title: "Download Queue"
                subtitle: "A unified queue for downloads, verification, extraction, and install steps."

                // Entry animation
                Component.onCompleted: {
                    opacity = 0
                    y = 20
                    entryAnimation.start()
                }

                SequentialAnimation {
                    id: entryAnimation
                    PauseAnimation { duration: 0 }
                    ParallelAnimation {
                        NumberAnimation {
                            target: parent
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: parent
                            property: "y"
                            from: 20
                            to: 0
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                    }
                }

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

                // Entry animation
                Component.onCompleted: {
                    opacity = 0
                    y = 20
                    entryAnimation2.start()
                }

                SequentialAnimation {
                    id: entryAnimation2
                    PauseAnimation { duration: root.staggerDelay }
                    ParallelAnimation {
                        NumberAnimation {
                            target: parent
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: parent
                            property: "y"
                            from: 20
                            to: 0
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                RowLayout {
                    anchors.fill: parent
                    spacing: 16

                    FluProgressRing {
                        id: progressRing
                        Layout.preferredWidth: 56
                        Layout.preferredHeight: 56
                        indeterminate: false
                        progressVisible: true
                        value: root.queueProgressValue()

                        // Smooth progress animation
                        Behavior on value {
                            NumberAnimation {
                                duration: 500
                                easing.type: Easing.OutCubic
                            }
                        }

                        // Scale animation when progress changes
                        SequentialAnimation {
                            id: progressPulseAnimation
                            NumberAnimation {
                                target: progressRing
                                property: "scale"
                                from: 1.0
                                to: 1.05
                                duration: 100
                            }
                            NumberAnimation {
                                target: progressRing
                                property: "scale"
                                from: 1.05
                                to: 1.0
                                duration: 150
                            }
                        }

                        onValueChanged: {
                            progressPulseAnimation.start()
                        }
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
                            id: queueProgressBar
                            width: parent.width
                            indeterminate: false
                            progressVisible: true
                            value: root.queueProgressValue()

                            // Smooth progress animation
                            Behavior on value {
                                NumberAnimation {
                                    duration: 400
                                    easing.type: Easing.OutCubic
                                }
                            }

                            // Glow effect on progress change
                            Rectangle {
                                anchors.fill: parent
                                radius: parent.radius
                                color: "transparent"
                                border.color: Qt.rgba(0.4, 0.7, 1, 0.6)
                                border.width: 2
                                opacity: 0

                                Behavior on opacity {
                                    NumberAnimation {
                                        duration: 300
                                    }
                                }
                            }
                        }
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 400
                title: "Queue Table"
                subtitle: "Structured queue rows rendered with FluTableView."

                // Entry animation
                Component.onCompleted: {
                    opacity = 0
                    y = 20
                    entryAnimation3.start()
                }

                SequentialAnimation {
                    id: entryAnimation3
                    PauseAnimation { duration: root.staggerDelay * 2 }
                    ParallelAnimation {
                        NumberAnimation {
                            target: parent
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: parent
                            property: "y"
                            from: 20
                            to: 0
                            duration: 300
                            easing.type: Easing.OutCubic
                        }
                    }
                }

                Component {
                    id: comTaskProgress
                    Item {
                        FluProgressBar {
                            id: taskProgressBar
                            anchors.verticalCenter: parent.verticalCenter
                            width: Math.max(parent.width - 12, 24)
                            x: 6
                            indeterminate: false
                            progressVisible: false
                            value: options && options.value !== undefined ? options.value : 0

                            // Smooth progress animation
                            Behavior on value {
                                NumberAnimation {
                                    duration: 300
                                    easing.type: Easing.OutCubic
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 10

                    FluTableView {
                        id: queueTable
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        columnSource: [
                            { "title": "Task", "dataIndex": "title", "width": 300 },
                            { "title": "Status", "dataIndex": "status", "width": 120 },
                            { "title": "Completed", "dataIndex": "completedSteps", "width": 90 },
                            { "title": "Steps", "dataIndex": "stepCount", "width": 80 },
                            { "title": "Progress", "dataIndex": "progress", "width": 80 },
                            { "title": "Bar", "dataIndex": "progressBar", "width": 170 }
                        ]
                        dataSource: root.queuePageRows()
                    }

                    FluPagination {
                        Layout.alignment: Qt.AlignHCenter
                        pageCurrent: root.queuePageCurrent
                        pageButtonCount: 5
                        itemCount: root.appViewModel ? root.appViewModel.taskCards.length : 0
                        __itemPerPage: root.queueItemsPerPage
                        onRequestPage: function(page) {
                            root.queuePageCurrent = page
                        }
                    }
                }
            }

            DawnCard {
                visible: appViewModel.taskCount === 0
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                title: "Idle Queue"
                subtitle: "No tasks are waiting. Queue a demo install from the content page."

                // Entry animation with fade
                Component.onCompleted: {
                    opacity = 0
                    scale = 0.95
                    idleEntryAnimation.start()
                }

                SequentialAnimation {
                    id: idleEntryAnimation
                    PauseAnimation { duration: root.staggerDelay * 3 }
                    ParallelAnimation {
                        NumberAnimation {
                            target: parent
                            property: "opacity"
                            from: 0
                            to: 1
                            duration: 400
                            easing.type: Easing.OutCubic
                        }
                        NumberAnimation {
                            target: parent
                            property: "scale"
                            from: 0.95
                            to: 1.0
                            duration: 400
                            easing.type: Easing.OutBack
                        }
                    }
                }

                // Pulse animation for idle state
                SequentialAnimation {
                    id: idlePulseAnimation
                    loops: Animation.Infinite
                    running: visible

                    NumberAnimation {
                        target: parent
                        property: "opacity"
                        from: 1
                        to: 0.7
                        duration: 2000
                        easing.type: Easing.InOutSine
                    }
                    NumberAnimation {
                        target: parent
                        property: "opacity"
                        from: 0.7
                        to: 1
                        duration: 2000
                        easing.type: Easing.InOutSine
                    }
                }

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

