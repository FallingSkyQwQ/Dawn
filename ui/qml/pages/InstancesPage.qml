import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components"

Item {
    id: root
    property var appViewModel

    function tabIndex(tabId) {
        var tabs = appViewModel.activeInstanceWorkbench.tabs || []
        for (var i = 0; i < tabs.length; ++i) {
            if (tabs[i].id === tabId) {
                return i
            }
        }
        return 0
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
                Layout.preferredHeight: 170
                title: "Instances"
                subtitle: "Instance-centered management, launch readiness, and workbench tabs."

                RowLayout {
                    anchors.fill: parent
                    spacing: 12

                    Button {
                        text: "Create Vanilla Instance"
                        onClicked: appViewModel.createInstance("Dawn Vanilla", "1.20.1", "none")
                    }

                    Button {
                        text: "Create Fabric Instance"
                        onClicked: appViewModel.createInstance("Dawn Fabric", "1.20.1", "fabric")
                    }

                    Button {
                        text: "Refresh"
                        onClicked: appViewModel.refresh()
                    }

                    Item { Layout.fillWidth: true }

                    Column {
                        spacing: 4

                        Text {
                            text: appViewModel.activeInstanceId.length > 0 ? "Active: " + appViewModel.activeInstanceId : "No active instance"
                            color: "#f5f8fb"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        Text {
                            text: appViewModel.activeInstanceWorkbench.instanceName.length > 0 ? appViewModel.activeInstanceWorkbench.instanceName : "Select an instance card to open its workbench"
                            color: "#9eb0c7"
                            font.pixelSize: 12
                        }
                    }
                }
            }

            DawnCard {
                id: dropCard
                Layout.fillWidth: true
                Layout.preferredHeight: 210
                title: "Local Drop Install"
                subtitle: "Drop a jar, zip, or mrpack to classify it and install it into the active instance."

                property bool dragActive: false

                Item {
                    anchors.fill: parent

                    Rectangle {
                        anchors.fill: parent
                        radius: 18
                        color: dropCard.dragActive ? Qt.rgba(0.18, 0.29, 0.44, 0.96) : Qt.rgba(1, 1, 1, 0.04)
                        border.color: dropCard.dragActive ? "#8ec5ff" : Qt.rgba(1, 1, 1, 0.08)
                        border.width: 1

                        DropArea {
                            anchors.fill: parent
                            keys: ["text/uri-list"]
                            onEntered: dropCard.dragActive = true
                            onExited: dropCard.dragActive = false
                            onDropped: function(drop) {
                                dropCard.dragActive = false
                                if (drop.urls.length > 0) {
                                    appViewModel.handleDroppedFile(drop.urls[0].toLocalFile(), appViewModel.activeInstanceId)
                                }
                            }
                        }

                        Column {
                            anchors.fill: parent
                            anchors.margins: 18
                            spacing: 10

                            Text {
                                text: dropCard.dragActive ? "Release to install into the active instance." : "Drag a local package here."
                                color: "#f5f8fb"
                                font.pixelSize: 18
                                font.bold: true
                            }

                            Text {
                                text: "Detected: " + (appViewModel.lastDroppedFileResult.detectedType || "unknown") + " | Status: " + (appViewModel.lastDroppedFileResult.status || "idle")
                                color: "#9eb0c7"
                                font.pixelSize: 12
                            }

                            Text {
                                text: appViewModel.lastDroppedFileResult.message || "The workflow will resolve the target directory and record a local lock."
                                color: "#dce5f0"
                                font.pixelSize: 13
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                visible: (appViewModel.lastDroppedFileResult.reasons || []).length > 0
                                text: "Reasons: " + (appViewModel.lastDroppedFileResult.reasons || []).join("  |  ")
                                color: "#92a3ba"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: "Target: " + (appViewModel.lastDroppedFileResult.targetInstanceId || appViewModel.activeInstanceId || "none")
                                color: "#92a3ba"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                DawnCard {
                    Layout.preferredWidth: 420
                    Layout.fillHeight: true
                    title: "Instance List"
                    subtitle: "Click a card to change the active workbench."

                    Column {
                        anchors.fill: parent
                        spacing: 10

                        Repeater {
                            model: appViewModel.instanceCards

                            delegate: Rectangle {
                                width: parent.width
                                height: 92
                                radius: 16
                                color: modelData.selected ? Qt.rgba(0.23, 0.34, 0.5, 0.95) : Qt.rgba(1, 1, 1, 0.03)
                                border.color: modelData.selected ? Qt.rgba(0.44, 0.64, 0.96, 0.55) : Qt.rgba(1, 1, 1, 0.05)
                                border.width: 1

                                MouseArea {
                                    anchors.fill: parent
                                    onClicked: appViewModel.setActiveInstance(modelData.id)
                                }

                                RowLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 12

                                    Rectangle {
                                        width: 48
                                        height: 48
                                        radius: 14
                                        color: modelData.selected ? "#8ec5ff" : "#66a3ff"

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

                                        Text {
                                            text: modelData.name
                                            color: "#f5f8fb"
                                            font.pixelSize: 16
                                            font.bold: true
                                        }

                                        Text {
                                            text: modelData.mcVersion + "  |  " + modelData.loader + "  |  " + modelData.health
                                            color: "#92a3ba"
                                            font.pixelSize: 12
                                        }

                                        Text {
                                            text: "Resources: " + modelData.resourceCount + "  |  Java: " + modelData.javaProfileId
                                            color: "#92a3ba"
                                            font.pixelSize: 12
                                        }
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
                                    text: "Use the create buttons above to write the first manifest."
                                    color: "#8ea0b7"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    title: "Instance Workbench"
                    subtitle: appViewModel.activeInstanceWorkbench.instanceName.length > 0 ? appViewModel.activeInstanceWorkbench.instanceName : "Overview, mods, packs, logs, runtime, and advanced settings."

                    Column {
                        anchors.fill: parent
                        spacing: 14

                        TabBar {
                            id: tabBar
                            width: parent.width
                            currentIndex: root.tabIndex(appViewModel.activeInstanceWorkbench.selectedTabId)

                            Repeater {
                                model: appViewModel.instanceWorkbenchTabs

                                delegate: TabButton {
                                    text: modelData.title
                                    checked: tabBar.currentIndex === index
                                    onClicked: appViewModel.setActiveInstanceTab(modelData.id)
                                }
                            }
                        }

                        StackLayout {
                            width: parent.width
                            currentIndex: root.tabIndex(appViewModel.activeInstanceWorkbench.selectedTabId)

                            Repeater {
                                model: appViewModel.instanceWorkbenchTabs

                                delegate: DawnCard {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    title: modelData.title
                                    subtitle: modelData.summary

                                    Column {
                                        anchors.fill: parent
                                        spacing: 10

                                        Text {
                                            text: "Instance: " + (appViewModel.activeInstanceWorkbench.instanceName.length > 0 ? appViewModel.activeInstanceWorkbench.instanceName : "None")
                                            color: "#f5f8fb"
                                            font.pixelSize: 18
                                            font.bold: true
                                        }

                                        Text {
                                            text: modelData.expert ? "Expert panel: advanced overrides are collapsed by default." : "Standard panel: core actions are kept in the foreground."
                                            color: "#dce5f0"
                                            font.pixelSize: 13
                                            wrapMode: Text.WordWrap
                                        }

                                        Text {
                                            text: root.workbenchTextFor(modelData.id)
                                            color: "#92a3ba"
                                            font.pixelSize: 12
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    function workbenchTextFor(tabId) {
        switch (tabId) {
        case "overview":
            return "Launch readiness, resource counts, and health are summarized here."
        case "mods":
            return "Mod enable/disable, dependency resolution, and update rollups belong here."
        case "resourcepacks":
            return "Resource pack ordering and pack-specific compatibility live here."
        case "shaderpacks":
            return "Shader installation and switching are isolated from core instance settings."
        case "worlds":
            return "World list, save exports, and restore points are grouped in this tab."
        case "logs":
            return "Launch logs and repair hints can be expanded without leaving the instance."
        case "runtime":
            return "Java runtime, memory budget, and loader bootstrap details stay visible."
        case "advanced":
            return "Expert-only flags, launch arguments, and overrides are intentionally collapsed."
        default:
            return "Workbench content placeholder."
        }
    }
}
