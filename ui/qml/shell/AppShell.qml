import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"
import "../pages"

Item {
    id: root
    property var appViewModel
    property int currentIndex: 0

    readonly property var navItems: [
        { "title": "首页", "subtitle": "Overview" },
        { "title": "实例", "subtitle": "Instances" },
        { "title": "内容中心", "subtitle": "Content" },
        { "title": "下载队列", "subtitle": "Queue" },
        { "title": "日志与修复", "subtitle": "Repair" },
        { "title": "设置", "subtitle": "Settings" }
    ]

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

                        Text {
                            text: "Dawn"
                            color: "#f4f7fb"
                            font.pixelSize: 28
                            font.bold: true
                        }

                        Text {
                            text: "Instance-centric launcher shell"
                            color: "#8798af"
                            font.pixelSize: 12
                        }
                    }

                    Repeater {
                        model: root.navItems

                        delegate: Button {
                            width: parent.width
                            height: 56
                            text: modelData.title + "\n" + modelData.subtitle
                            checkable: true
                            checked: index === root.currentIndex
                            padding: 14
                            font.pixelSize: 15

                            background: Rectangle {
                                radius: 16
                                color: parent.checked ? Qt.rgba(0.26, 0.37, 0.55, 0.85) : Qt.rgba(1, 1, 1, 0.03)
                                border.color: parent.checked ? Qt.rgba(0.48, 0.64, 0.98, 0.45) : Qt.rgba(1, 1, 1, 0.05)
                            }

                            contentItem: Column {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 2

                                Text {
                                    text: modelData.title
                                    color: "#f5f8fb"
                                    font.pixelSize: 16
                                    font.bold: true
                                }

                                Text {
                                    text: modelData.subtitle
                                    color: "#93a4bb"
                                    font.pixelSize: 11
                                }
                            }

                            onClicked: root.currentIndex = index
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

                            Text {
                                text: "FluentUIbi switch is reserved for the upstream submodule."
                                color: "#c5d0df"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }
                            Text {
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

                StackLayout {
                    anchors.fill: parent
                    anchors.margins: 24
                    currentIndex: root.currentIndex

                    HomePage { id: homePage; appViewModel: root.appViewModel }
                    InstancesPage { id: instancesPage; appViewModel: root.appViewModel }
                    ContentCenterPage { id: contentPage; appViewModel: root.appViewModel }
                    DownloadQueuePage { id: queuePage; appViewModel: root.appViewModel }
                    LogsRepairPage { id: repairPage; appViewModel: root.appViewModel }
                    SettingsPage { id: settingsPage; appViewModel: root.appViewModel }
                }
            }
        }
    }
}
