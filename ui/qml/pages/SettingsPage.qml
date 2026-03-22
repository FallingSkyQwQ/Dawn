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
                Layout.preferredHeight: 150
                title: "Settings"
                subtitle: "Theme, windowing, downloads, caching, Java, accounts, and backups."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        text: "The initial pass focuses on architecture and persistence rather than every panel being interactive."
                        color: "#dce5f0"
                        font.pixelSize: 14
                    }

                    Text {
                        text: "All settings groups are represented so the information architecture matches the product brief."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: 2
                columnSpacing: 16
                rowSpacing: 16

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    title: "Theme"
                    subtitle: "Dark, light, and accent-aware styling."

                    Text {
                        anchors.fill: parent
                        text: "Follow system theme, instance accent color, and density mode."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    title: "Download"
                    subtitle: "Concurrency, caching, and verification."

                    Text {
                        anchors.fill: parent
                        text: "The queue and repository layers already exist, while the online downloader is reserved for the next stage."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    title: "Java"
                    subtitle: "Discovered runtimes and per-instance profiles."

                    Text {
                        anchors.fill: parent
                        text: "The launch runtime exposes a placeholder command builder so Java policies can be wired later without breaking the shell."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 180
                    title: "Backups"
                    subtitle: "Snapshots and restore points."

                    Text {
                        anchors.fill: parent
                        text: "Backup policy and archive support are intentionally staged after the core instance and launch flows."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
