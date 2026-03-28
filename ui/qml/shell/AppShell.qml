import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"
import "../pages"
import "../dialogs"

Item {
    id: root
    property var appViewModel
    property int currentIndex: 0
    property int previousIndex: 0

    readonly property bool showWizard: root.appViewModel && root.appViewModel.firstLaunchVisible

    // Animation durations
    readonly property int pageTransitionDuration: 260  // 240-300ms for page transitions
    readonly property int microInteractionDuration: 140  // 120-160ms for micro interactions
    readonly property int panelSwitchDuration: 200  // 180-220ms for panel switching

    // Track navigation direction
    onCurrentIndexChanged: {
        previousIndex = currentIndex
    }

    Connections {
        target: root.appViewModel
        function onNavigateToPageRequested(pageIndex) {
            if (pageIndex >= 0) {
                root.currentIndex = pageIndex
            }
        }
    }

    // 全局拖拽检测区域（当窗口获得拖拽时显示 DropZone）
    DropArea {
        id: globalDropArea
        anchors.fill: parent
        z: 1

        onEntered: function(drag) {
            if (drag.hasUrls && dropZone) {
                // 检查是否有支持的文件
                var hasSupportedFile = false
                for (var i = 0; i < drag.urls.length; i++) {
                    var url = drag.urls[i]
                    var filePath = url.toString().toLowerCase()
                    if (filePath.endsWith(".jar") || filePath.endsWith(".zip") || filePath.endsWith(".mrpack")) {
                        hasSupportedFile = true
                        break
                    }
                }
                if (hasSupportedFile) {
                    dropZone.show()
                }
            }
        }
    }

    // 拖拽放置区域
    DropZone {
        id: dropZone
        z: 9998

        onFilesDropped: function(fileUrls) {
            dragDropDialog.reset()
            dragDropDialog.analyzeFiles(fileUrls)
            dragDropDialog.open()
        }
    }

    // 拖拽安装对话框
    DragDropInstallDialog {
        id: dragDropDialog
        appViewModel: root.appViewModel
    }

    StackLayout {
        anchors.fill: parent
        currentIndex: root.showWizard ? 0 : 1

        FirstLaunchWizardPage {
            appViewModel: root.appViewModel
        }

        Item {
            Rectangle {
                anchors.fill: parent
                radius: 0
                color: "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: 20
                    spacing: 20

                    Rectangle {
                        Layout.preferredWidth: 250
                        Layout.fillHeight: true
                        radius: 24
                        color: Qt.rgba(0.08, 0.11, 0.15, 0.92)
                        border.color: Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        Column {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 18

                            Column {
                                spacing: 6

                                FluText {
                                    text: "Dawn"
                                    color: "#f4f7fb"
                                    font.pixelSize: 28
                                    font.bold: true
                                }

                                FluText {
                                    text: "Instance-centric launcher shell"
                                    color: "#8798af"
                                    font.pixelSize: 12
                                }
                            }

                            Repeater {
                                model: [
                                    { "title": "首页", "subtitle": "Overview" },
                                    { "title": "实例", "subtitle": "Instances" },
                                    { "title": "内容中心", "subtitle": "Content" },
                                    { "title": "下载队列", "subtitle": "Queue" },
                                    { "title": "日志与修复", "subtitle": "Repair" },
                                    { "title": "设置", "subtitle": "Settings" }
                                ]

                                delegate: FluButton {
                                    id: navButton
                                    width: parent.width
                                    height: 56
                                    text: modelData.title + "\n" + modelData.subtitle
                                    checkable: true
                                    checked: index === root.currentIndex
                                    padding: 14
                                    font.pixelSize: 15

                                    // Hover animation properties
                                    property real hoverScale: 1.0
                                    property real hoverElevation: 0

                                    background: Rectangle {
                                        id: navBg
                                        radius: 16
                                        color: parent.checked ? Qt.rgba(0.26, 0.37, 0.55, 0.85) : Qt.rgba(1, 1, 1, 0.03)
                                        border.color: parent.checked ? Qt.rgba(0.48, 0.64, 0.98, 0.45) : Qt.rgba(1, 1, 1, 0.05)
                                        border.width: parent.checked ? 2 : 1

                                        // Scale animation on hover
                                        scale: navButton.hoverScale

                                        Behavior on scale {
                                            NumberAnimation {
                                                duration: root.microInteractionDuration
                                                easing.type: Easing.OutCubic
                                            }
                                        }

                                        Behavior on color {
                                            ColorAnimation {
                                                duration: root.microInteractionDuration
                                            }
                                        }

                                        Behavior on border.color {
                                            ColorAnimation {
                                                duration: root.microInteractionDuration
                                            }
                                        }

                                        // Shadow effect on hover
                                        Rectangle {
                                            id: navShadow
                                            anchors.fill: parent
                                            radius: parent.radius
                                            color: "transparent"
                                            opacity: navButton.hoverElevation

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: parent.radius
                                                color: Qt.rgba(0, 0, 0, 0.3)
                                                anchors.margins: -2
                                            }

                                            Behavior on opacity {
                                                NumberAnimation {
                                                    duration: root.microInteractionDuration
                                                    easing.type: Easing.OutCubic
                                                }
                                            }
                                        }
                                    }

                                    contentItem: Column {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 2

                                        FluText {
                                            text: modelData.title
                                            color: "#f5f8fb"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }

                                        FluText {
                                            text: modelData.subtitle
                                            color: "#93a4bb"
                                            font.pixelSize: 11
                                        }
                                    }

                                    onClicked: root.currentIndex = index

                                    // Hover handlers
                                    onHoveredChanged: {
                                        hoverScale = hovered ? 1.02 : 1.0
                                        hoverElevation = hovered ? 0.3 : 0
                                    }

                                    // Entry animation for each button
                                    Component.onCompleted: {
                                        opacity = 0
                                        x = -20
                                        entryAnimation.start()
                                    }

                                    SequentialAnimation {
                                        id: entryAnimation
                                        PauseAnimation { duration: index * 40 }
                                        ParallelAnimation {
                                            NumberAnimation {
                                                target: navButton
                                                property: "opacity"
                                                from: 0
                                                to: 1
                                                duration: 200
                                                easing.type: Easing.OutCubic
                                            }
                                            NumberAnimation {
                                                target: navButton
                                                property: "x"
                                                from: -20
                                                to: 0
                                                duration: 200
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }
                                }
                            }

                            Item { Layout.fillHeight: true }

                            DawnCard {
                                width: parent.width
                                height: 126
                                title: "Build Mode"
                                subtitle: "Qt/QML shell with headless fallback."

                                Column {
                                    anchors.fill: parent
                                    spacing: 6

                                    FluText {
                                        text: "FluentUI integration is provided by the upstream submodule."
                                        color: "#c5d0df"
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }
                                    FluText {
                                        text: "This repository keeps the launcher usable even when the submodule is absent."
                                        color: "#93a4bb"
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        radius: 24
                        color: Qt.rgba(0.06, 0.08, 0.11, 0.88)
                        border.color: Qt.rgba(1, 1, 1, 0.06)
                        border.width: 1

                        StackView {
                            id: pageStack
                            anchors.fill: parent
                            anchors.margins: 24
                            initialItem: homePage

                            // Page transition animations
                            replaceEnter: Transition {
                                ParallelAnimation {
                                    NumberAnimation {
                                        property: "x"
                                        from: root.currentIndex > root.previousIndex ? 40 : -40
                                        to: 0
                                        duration: root.pageTransitionDuration
                                        easing.type: Easing.OutCubic
                                    }
                                    NumberAnimation {
                                        property: "opacity"
                                        from: 0
                                        to: 1
                                        duration: root.pageTransitionDuration
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }

                            replaceExit: Transition {
                                ParallelAnimation {
                                    NumberAnimation {
                                        property: "x"
                                        from: 0
                                        to: root.currentIndex > root.previousIndex ? -40 : 40
                                        duration: root.pageTransitionDuration
                                        easing.type: Easing.OutCubic
                                    }
                                    NumberAnimation {
                                        property: "opacity"
                                        from: 1
                                        to: 0
                                        duration: root.pageTransitionDuration
                                        easing.type: Easing.OutCubic
                                    }
                                }
                            }

                            Component {
                                id: homePage
                                HomePage { appViewModel: root.appViewModel }
                            }
                            Component {
                                id: instancesPage
                                InstancesPage { appViewModel: root.appViewModel }
                            }
                            Component {
                                id: contentPage
                                ContentCenterPage { appViewModel: root.appViewModel }
                            }
                            Component {
                                id: queuePage
                                DownloadQueuePage { appViewModel: root.appViewModel }
                            }
                            Component {
                                id: repairPage
                                LogsRepairPage { appViewModel: root.appViewModel }
                            }
                            Component {
                                id: settingsPage
                                SettingsPage { appViewModel: root.appViewModel }
                            }

                            // Handle page switching with animation
                            onCurrentIndexChanged: {
                                var pages = [homePage, instancesPage, contentPage, queuePage, repairPage, settingsPage]
                                if (root.currentIndex >= 0 && root.currentIndex < pages.length) {
                                    pageStack.replace(pages[root.currentIndex])
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

