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
                Layout.preferredHeight: 170
                title: "Content Center"
                subtitle: "Modrinth-style search, version matching, and install plans."

                Column {
                    anchors.fill: parent
                    spacing: 10

                    RowLayout {
                        width: parent.width
                        spacing: 10

                        TextField {
                            Layout.fillWidth: true
                            placeholderText: "Search mods, modpacks, resource packs, shaders"
                        }

                        ComboBox {
                            model: [ "Mod", "Modpack", "Resourcepack", "Shader" ]
                        }

                        Button {
                            text: "Queue Demo Install"
                            onClicked: appViewModel.enqueueDemoTask("Install content from provider stub")
                        }
                    }

                    Text {
                        text: "The provider layer is stubbed locally, but the data model and install plan flow are already in place."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 340
                    title: "Search Results"
                    subtitle: "Native cards that can later bind to a real Modrinth API."

                    Column {
                        anchors.fill: parent
                        spacing: 12

                        Repeater {
                            model: [
                                { "title": "Sodium", "type": "Shader/Perf", "summary": "Performance-focused rendering stack." },
                                { "title": "Fabric API", "type": "Library", "summary": "Common dependency layer for Fabric packs." },
                                { "title": "Essential Modpack", "type": "Modpack", "summary": "Demo packaged workflow for instance creation." }
                            ]

                            delegate: Rectangle {
                                width: parent.width
                                height: 86
                                radius: 14
                                color: Qt.rgba(1, 1, 1, 0.03)
                                border.color: Qt.rgba(1, 1, 1, 0.05)

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 12

                                    Rectangle {
                                        width: 48
                                        height: 48
                                        radius: 14
                                        color: "#66a3ff"
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.title[0]
                                            color: "white"
                                            font.pixelSize: 20
                                            font.bold: true
                                        }
                                    }

                                    Column {
                                        Layout.fillWidth: true
                                        spacing: 2
                                        Text { text: modelData.title; color: "#f5f8fb"; font.pixelSize: 15; font.bold: true }
                                        Text { text: modelData.type; color: "#9eb0c7"; font.pixelSize: 12 }
                                        Text { text: modelData.summary; color: "#8ea0b7"; font.pixelSize: 12 }
                                    }

                                    Button {
                                        text: "Install"
                                        enabled: false
                                    }
                                }
                            }
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 340
                    title: "Install Flow"
                    subtitle: "Dependency resolution, hash verification, deploy, rollback."

                    Column {
                        anchors.fill: parent
                        spacing: 12

                        Text {
                            text: "1. Search provider\n2. Resolve dependencies\n3. Build install plan\n4. Deploy into the target instance"
                            color: "#dce5f0"
                            font.pixelSize: 14
                            lineHeight: 1.3
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: "The current implementation preserves the interfaces needed for Modrinth, local packs, and future providers."
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            width: parent.width
                            height: 88
                            radius: 14
                            color: Qt.rgba(0.16, 0.21, 0.28, 0.8)
                            border.color: Qt.rgba(0.4, 0.55, 0.8, 0.35)

                            Column {
                                anchors.fill: parent
                                anchors.margins: 14
                                spacing: 6
                                Text { text: "Selected target"; color: "#9eb0c7"; font.pixelSize: 12 }
                                Text { text: appViewModel.primaryInstanceId.length > 0 ? appViewModel.primaryInstanceId : "No target selected"; color: "#f5f8fb"; font.pixelSize: 16; font.bold: true }
                            }
                        }
                    }
                }
            }
        }
    }
}
