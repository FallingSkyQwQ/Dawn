import QtQuick
import QtQuick.Layouts
import FluentUI 1.0

Rectangle {
    id: root

    property string selectedVersion: ""
    property string selectedVersionType: "release"
    property var versionList: []

    signal versionSelected(string version, string versionType)

    color: "transparent"

    // Sample version data - in real app this would come from backend
    property var releaseVersions: [
        { version: "1.21.4", date: "2024-12-03", java: "Java 21" },
        { version: "1.21.3", date: "2024-10-23", java: "Java 21" },
        { version: "1.21.1", date: "2024-08-08", java: "Java 21" },
        { version: "1.21", date: "2024-06-13", java: "Java 21" },
        { version: "1.20.6", date: "2024-04-29", java: "Java 17" },
        { version: "1.20.4", date: "2023-12-07", java: "Java 17" },
        { version: "1.20.1", date: "2023-06-12", java: "Java 17" },
        { version: "1.20", date: "2023-06-07", java: "Java 17" },
        { version: "1.19.4", date: "2023-03-14", java: "Java 17" },
        { version: "1.19.2", date: "2022-08-05", java: "Java 17" },
        { version: "1.18.2", date: "2022-02-28", java: "Java 17" },
        { version: "1.16.5", date: "2021-01-15", java: "Java 8/11" }
    ]

    property var snapshotVersions: [
        { version: "25w09b", date: "2025-02-26", java: "Java 21" },
        { version: "25w09a", date: "2025-02-26", java: "Java 21" },
        { version: "25w08a", date: "2025-02-19", java: "Java 21" },
        { version: "25w07a", date: "2025-02-12", java: "Java 21" },
        { version: "25w06a", date: "2025-02-05", java: "Java 21" }
    ]

    property var oldBetaVersions: [
        { version: "b1.7.3", date: "2011-07-08", java: "Java 6" },
        { version: "b1.7.2", date: "2011-07-08", java: "Java 6" },
        { version: "b1.6.6", date: "2011-05-31", java: "Java 6" },
        { version: "b1.5_01", date: "2011-04-20", java: "Java 6" },
        { version: "b1.4_01", date: "2011-04-05", java: "Java 6" }
    ]

    property var oldAlphaVersions: [
        { version: "a1.2.6", date: "2010-12-03", java: "Java 6" },
        { version: "a1.2.5", date: "2010-12-01", java: "Java 6" },
        { version: "a1.2.4_01", date: "2010-11-30", java: "Java 6" },
        { version: "a1.2.3_04", date: "2010-11-26", java: "Java 6" },
        { version: "a1.2.2", date: "2010-11-10", java: "Java 6" }
    ]

    function getCurrentVersions() {
        switch (tabView.currentIndex) {
            case 0: return root.releaseVersions
            case 1: return root.snapshotVersions
            case 2: return root.oldBetaVersions
            case 3: return root.oldAlphaVersions
            default: return root.releaseVersions
        }
    }

    function getCurrentVersionType() {
        switch (tabView.currentIndex) {
            case 0: return "release"
            case 1: return "snapshot"
            case 2: return "old_beta"
            case 3: return "old_alpha"
            default: return "release"
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        // Version type tabs
        FluTabView {
            id: tabView
            Layout.fillWidth: true
            Layout.preferredHeight: 320

            FluTabItem {
                title: "正式版 (Release)"
                contentItem: versionListComponent
            }

            FluTabItem {
                title: "快照 (Snapshot)"
                contentItem: versionListComponent
            }

            FluTabItem {
                title: "旧版 Beta"
                contentItem: versionListComponent
            }

            FluTabItem {
                title: "旧版 Alpha"
                contentItem: versionListComponent
            }
        }

        // Selected version display
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            radius: 8
            color: Qt.rgba(1, 1, 1, 0.05)
            border.color: Qt.rgba(1, 1, 1, 0.1)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 16

                FluText {
                    text: "已选择:"
                    color: "#8ea0b7"
                    font.pixelSize: 13
                }

                FluText {
                    text: root.selectedVersion.length > 0 ? root.selectedVersion : "请从列表中选择一个版本"
                    color: root.selectedVersion.length > 0 ? "#ffffff" : "#8ea0b7"
                    font.pixelSize: 14
                    font.bold: root.selectedVersion.length > 0
                }

                Item { Layout.fillWidth: true }

                FluText {
                    visible: root.selectedVersion.length > 0
                    text: "类型: " + root.selectedVersionType
                    color: "#dce5f0"
                    font.pixelSize: 12
                }
            }
        }
    }

    Component {
        id: versionListComponent

        Rectangle {
            color: "transparent"

            ListView {
                id: versionListView
                anchors.fill: parent
                clip: true
                spacing: 4
                model: root.getCurrentVersions()

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 44
                    radius: 6
                    color: modelData.version === root.selectedVersion ?
                           Qt.rgba(0.3, 0.5, 0.8, 0.4) : Qt.rgba(1, 1, 1, 0.03)
                    border.color: modelData.version === root.selectedVersion ?
                                  Qt.rgba(0.5, 0.7, 1, 0.5) : "transparent"

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.selectedVersion = modelData.version
                            root.selectedVersionType = root.getCurrentVersionType()
                            root.versionSelected(modelData.version, root.selectedVersionType)
                        }
                    }

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 10
                        spacing: 16

                        FluText {
                            text: modelData.version
                            color: "#f5f8fb"
                            font.pixelSize: 14
                            font.bold: true
                            Layout.preferredWidth: 100
                        }

                        FluText {
                            text: modelData.date
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            Layout.preferredWidth: 100
                        }

                        FluText {
                            text: modelData.java
                            color: "#9ce3b6"
                            font.pixelSize: 12
                        }

                        Item { Layout.fillWidth: true }

                        FluIcon {
                            visible: modelData.version === root.selectedVersion
                            iconSource: FluentIcons.Accept
                            iconSize: 16
                            iconColor: FluTheme.primaryColor
                        }
                    }
                }

                // Recommended badge for first item
                section.property: ""
                section.criteria: ViewSection.FullString
            }
        }
    }
}
