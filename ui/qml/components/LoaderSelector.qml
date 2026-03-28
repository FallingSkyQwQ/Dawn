import QtQuick
import QtQuick.Layouts
import FluentUI 1.0

Rectangle {
    id: root

    property string selectedLoader: "none"
    property string selectedLoaderVersion: ""
    property string mcVersion: ""

    signal loaderSelected(string loader, string loaderVersion)

    color: "transparent"

    // Loader definitions with compatibility info
    property var loaders: [
        {
            id: "none",
            name: "无 (Vanilla)",
            description: "原版 Minecraft，无需加载器",
            compatible: true,
            versions: ["最新"]
        },
        {
            id: "fabric",
            name: "Fabric",
            description: "轻量级 Mod 加载器，更新快，兼容性好",
            compatible: true,
            versions: ["0.16.10", "0.16.9", "0.16.8", "0.16.7", "0.16.6"]
        },
        {
            id: "quilt",
            name: "Quilt",
            description: "Fabric 的分支，提供更好的 Mod 兼容性",
            compatible: true,
            versions: ["0.28.0", "0.27.1", "0.27.0", "0.26.0", "0.25.0"]
        },
        {
            id: "forge",
            name: "Forge",
            description: "老牌 Mod 加载器，Mod 生态丰富",
            compatible: true,
            versions: ["52.0.16", "52.0.15", "52.0.14", "52.0.13", "52.0.12"]
        },
        {
            id: "neoforge",
            name: "NeoForge",
            description: "Forge 的分支，Forge 1.20.1+ 的替代方案",
            compatible: true,
            versions: ["21.4.83", "21.4.82", "21.4.81", "21.4.80", "21.4.79"]
        },
        {
            id: "optifine",
            name: "OptiFine",
            description: "光影和性能优化（需单独安装）",
            compatible: true,
            versions: ["HD_U_J4", "HD_U_J3", "HD_U_J2", "HD_U_J1", "HD_U_I9"]
        }
    ]

    function getLoaderById(id) {
        for (var i = 0; i < root.loaders.length; i++) {
            if (root.loaders[i].id === id) {
                return root.loaders[i]
            }
        }
        return root.loaders[0]
    }

    function getCurrentLoader() {
        return getLoaderById(root.selectedLoader)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 16

        // Loader selection grid
        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: 12
            rowSpacing: 12

            Repeater {
                model: root.loaders

                delegate: Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                    radius: 10
                    color: modelData.id === root.selectedLoader ?
                           Qt.rgba(0.3, 0.5, 0.8, 0.4) : Qt.rgba(1, 1, 1, 0.05)
                    border.color: modelData.id === root.selectedLoader ?
                                  Qt.rgba(0.5, 0.7, 1, 0.6) : Qt.rgba(1, 1, 1, 0.1)
                    border.width: modelData.id === root.selectedLoader ? 2 : 1

                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            root.selectedLoader = modelData.id
                            root.selectedLoaderVersion = modelData.versions[0]
                            root.loaderSelected(root.selectedLoader, root.selectedLoaderVersion)
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 12
                        spacing: 4

                        RowLayout {
                            spacing: 8

                            FluRadioButton {
                                checked: modelData.id === root.selectedLoader
                                text: ""
                                enabled: false
                            }

                            FluText {
                                text: modelData.name
                                color: "#f5f8fb"
                                font.pixelSize: 14
                                font.bold: true
                            }

                            Item { Layout.fillWidth: true }

                            FluIcon {
                                visible: modelData.id === root.selectedLoader
                                iconSource: FluentIcons.Accept
                                iconSize: 16
                                iconColor: FluTheme.primaryColor
                            }
                        }

                        FluText {
                            text: modelData.description
                            color: "#8ea0b7"
                            font.pixelSize: 11
                            wrapMode: Text.WordWrap
                            Layout.fillWidth: true
                        }
                    }
                }
            }
        }

        // Loader version selection (only show when loader is not "none")
        Rectangle {
            visible: root.selectedLoader !== "none"
            Layout.fillWidth: true
            Layout.preferredHeight: 180
            radius: 10
            color: Qt.rgba(1, 1, 1, 0.03)
            border.color: Qt.rgba(1, 1, 1, 0.08)

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                RowLayout {
                    spacing: 8

                    FluText {
                        text: "选择 " + getCurrentLoader().name + " 版本"
                        color: "#f5f8fb"
                        font.pixelSize: 13
                        font.bold: true
                    }

                    Item { Layout.fillWidth: true }

                    FluText {
                        text: "推荐: " + getCurrentLoader().versions[0]
                        color: "#9ce3b6"
                        font.pixelSize: 11
                    }
                }

                // Version list
                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    spacing: 4
                    model: getCurrentLoader().versions

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: 36
                        radius: 6
                        color: modelData === root.selectedLoaderVersion ?
                               Qt.rgba(0.3, 0.5, 0.8, 0.3) : "transparent"

                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                root.selectedLoaderVersion = modelData
                                root.loaderSelected(root.selectedLoader, root.selectedLoaderVersion)
                            }
                        }

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 10

                            FluRadioButton {
                                checked: modelData === root.selectedLoaderVersion
                                text: ""
                                enabled: false
                            }

                            FluText {
                                text: modelData
                                color: modelData === root.selectedLoaderVersion ? "#ffffff" : "#dce5f0"
                                font.pixelSize: 12
                            }

                            Item { Layout.fillWidth: true }

                            FluText {
                                visible: index === 0
                                text: "推荐"
                                color: "#9ce3b6"
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }
        }

        // Compatibility info
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            radius: 8
            color: Qt.rgba(1, 1, 1, 0.05)
            border.color: Qt.rgba(1, 1, 1, 0.1)

            RowLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 12

                FluIcon {
                    iconSource: FluentIcons.Info
                    iconSize: 16
                    iconColor: "#66a3ff"
                }

                ColumnLayout {
                    spacing: 2

                    FluText {
                        text: "兼容性信息"
                        color: "#f5f8fb"
                        font.pixelSize: 12
                        font.bold: true
                    }

                    FluText {
                        text: root.mcVersion.length > 0 ?
                              "Minecraft " + root.mcVersion + " 与 " + getCurrentLoader().name + " 兼容" :
                              "请先选择 Minecraft 版本以查看兼容性"
                        color: "#8ea0b7"
                        font.pixelSize: 11
                    }
                }
            }
        }

        // Selected loader summary
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
                    text: getCurrentLoader().name + (root.selectedLoaderVersion.length > 0 && root.selectedLoader !== "none" ? " " + root.selectedLoaderVersion : "")
                    color: "#ffffff"
                    font.pixelSize: 14
                    font.bold: true
                }
            }
        }
    }
}
