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
                Layout.preferredHeight: 160
                title: "First Launch Wizard"
                subtitle: "Complete the onboarding flow before entering the main shell."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    Text {
                        text: "Welcome to Dawn. This wizard covers the initial setup path in four steps."
                        color: "#f5f8fb"
                        font.pixelSize: 17
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: "Step through the data root, cache, and Java defaults, then seal the flow."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 110
                title: "Progress"
                subtitle: "Current wizard step and remaining stages."

                RowLayout {
                    anchors.fill: parent
                    spacing: 12

                    Repeater {
                        model: appViewModel.wizardSteps

                        delegate: Rectangle {
                            Layout.fillWidth: true
                            height: 72
                            radius: 14
                            color: modelData.active ? Qt.rgba(0.26, 0.37, 0.55, 0.85) : Qt.rgba(1, 1, 1, 0.03)
                            border.color: modelData.completed ? Qt.rgba(0.48, 0.64, 0.98, 0.45) : Qt.rgba(1, 1, 1, 0.05)

                            Column {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 2

                                Text {
                                    text: (modelData.index + 1) + ". " + modelData.title
                                    color: "#f5f8fb"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                Text {
                                    text: modelData.summary
                                    color: "#93a4bb"
                                    font.pixelSize: 10
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }
                    }
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.preferredHeight: 340
                width: parent.width
                currentIndex: appViewModel.wizardStepIndex

                Item {
                    width: parent.width
                    height: 290

                    DawnCard {
                        anchors.fill: parent
                        title: "Welcome"
                        subtitle: "Confirm the launcher context and the base flow."

                        Column {
                            anchors.fill: parent
                            spacing: 14

                            Text {
                                text: "Dawn keeps instance state, content installs, and repair actions in separate workflows."
                                color: "#dce5f0"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: "Next, review where Dawn stores data and how its cache is maintained."
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                width: parent.width
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Next"
                                    onClicked: appViewModel.nextWizardStep()
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 320

                    DawnCard {
                        anchors.fill: parent
                        title: "Data Path"
                        subtitle: "Review the data root, cache location, and disk health."

                        Column {
                            anchors.fill: parent
                            spacing: 12

                            Text {
                                text: "Data root: " + appViewModel.dataRoot
                                color: "#f5f8fb"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: "Cache path: " + appViewModel.cachePath
                                color: "#f5f8fb"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: "Disk state: " + appViewModel.diskSpaceStatus.statusLabel + " | Available " + appViewModel.diskSpaceStatus.availableDisplay + " | Threshold " + appViewModel.diskSpaceStatus.thresholdDisplay
                                color: appViewModel.diskSpaceStatus.low ? "#f2c5ba" : "#9ce3b6"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: appViewModel.lowDiskWarning.length > 0 ? appViewModel.lowDiskWarning : "Storage is within the configured threshold."
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                width: parent.width
                                Button {
                                    text: "Back"
                                    onClicked: appViewModel.previousWizardStep()
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Next"
                                    onClicked: appViewModel.nextWizardStep()
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 320

                    DawnCard {
                        anchors.fill: parent
                        title: "Java Strategy"
                        subtitle: "Select how Dawn should resolve Java before launch."

                        Column {
                            anchors.fill: parent
                            spacing: 12

                            Text {
                                text: "Current strategy: " + appViewModel.javaStrategy
                                color: "#f5f8fb"
                                font.pixelSize: 14
                            }

                            RowLayout {
                                width: parent.width
                                spacing: 8

                                Repeater {
                                    model: [
                                        { "id": "auto", "title": "Auto" },
                                        { "id": "bundled", "title": "Bundled" },
                                        { "id": "custom-path", "title": "Custom" },
                                        { "id": "downloaded", "title": "Downloaded" }
                                    ]

                                    delegate: Button {
                                        text: modelData.title
                                        checkable: true
                                        checked: appViewModel.javaStrategy === modelData.id
                                        onClicked: appViewModel.setJavaStrategy(modelData.id)
                                    }
                                }
                            }

                            Text {
                                text: "Auto is the safest default. Bundled and custom paths are useful when you already manage a runtime."
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                width: parent.width
                                Button {
                                    text: "Back"
                                    onClicked: appViewModel.previousWizardStep()
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Next"
                                    onClicked: appViewModel.nextWizardStep()
                                }
                            }
                        }
                    }
                }

                Item {
                    width: parent.width
                    height: 310

                    DawnCard {
                        anchors.fill: parent
                        title: "Finish"
                        subtitle: "Seal the setup and enter the launcher shell."

                        Column {
                            anchors.fill: parent
                            spacing: 12

                            Text {
                                text: "First launch completion will hide this wizard on the next route update."
                                color: "#dce5f0"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            Text {
                                text: "Data root: " + appViewModel.dataRoot + "\nCache path: " + appViewModel.cachePath + "\nJava strategy: " + appViewModel.javaStrategy
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                width: parent.width
                                Button {
                                    text: "Back"
                                    onClicked: appViewModel.previousWizardStep()
                                }
                                Item { Layout.fillWidth: true }
                                Button {
                                    text: "Finish Setup"
                                    onClicked: appViewModel.completeFirstLaunch()
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
