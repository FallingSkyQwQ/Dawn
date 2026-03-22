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
                        text: "The initial pass now exposes persistent usability controls instead of static copy."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }

                    Text {
                        text: "First launch completion, novice/advanced mode, low-disk threshold, and cache maintenance are wired through the view model."
                        color: "#8ea0b7"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
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
                    Layout.preferredHeight: 190
                    title: "Experience Mode"
                    subtitle: "Switch between novice and advanced controls."

                    Column {
                        anchors.fill: parent
                        spacing: 10

                        Switch {
                            text: appViewModel.uiMode === "advanced" ? "Advanced mode" : "Novice mode"
                            checked: appViewModel.uiMode === "advanced"
                            onToggled: appViewModel.setUiMode(checked ? "advanced" : "novice")
                        }

                        Text {
                            text: appViewModel.uiMode === "advanced"
                                  ? "Advanced mode exposes the full control surface."
                                  : "Novice mode keeps the default flow compact."
                            color: "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 190
                    title: "First Launch"
                    subtitle: "Track onboarding completion after the wizard flow."

                    Column {
                        anchors.fill: parent
                        spacing: 10

                        Text {
                            text: appViewModel.firstLaunchCompleted ? "First launch is marked complete." : "First launch wizard is still active."
                            color: "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        Button {
                            text: appViewModel.firstLaunchCompleted ? "Already Completed" : "Complete First Launch"
                            enabled: !appViewModel.firstLaunchCompleted
                            onClicked: appViewModel.completeFirstLaunch()
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 270
                    title: "Low Disk Threshold"
                    subtitle: "Warn when the data root free space is too low."

                    Column {
                        anchors.fill: parent
                        spacing: 10

                        RowLayout {
                            width: parent.width
                            spacing: 10

                            Text {
                                text: "Threshold (GB)"
                                color: "#dce5f0"
                                font.pixelSize: 13
                            }

                            SpinBox {
                                id: thresholdSpin
                                from: 0
                                to: 1024
                                value: appViewModel.lowDiskThresholdGb
                                editable: true
                                onValueModified: appViewModel.setLowDiskThresholdGb(value)
                            }
                        }

                        Text {
                            text: appViewModel.lowDiskWarning.length > 0 ? appViewModel.lowDiskWarning : "No low disk warning at the moment."
                            color: appViewModel.lowDiskWarning.length > 0 ? "#f2c5ba" : "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: "Probe path: " + appViewModel.diskSpaceStatus.path
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Rectangle {
                            width: parent.width
                            height: 1
                            color: Qt.rgba(1, 1, 1, 0.06)
                        }

                        RowLayout {
                            width: parent.width
                            spacing: 12

                            Column {
                                Layout.fillWidth: true
                                spacing: 4
                                Text { text: "Available"; color: "#8ea0b7"; font.pixelSize: 11 }
                                Text { text: appViewModel.diskSpaceStatus.availableDisplay; color: "#f5f8fb"; font.pixelSize: 13 }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 4
                                Text { text: "Threshold"; color: "#8ea0b7"; font.pixelSize: 11 }
                                Text { text: appViewModel.diskSpaceStatus.thresholdDisplay; color: "#f5f8fb"; font.pixelSize: 13 }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 4
                                Text { text: "State"; color: "#8ea0b7"; font.pixelSize: 11 }
                                Text { text: appViewModel.diskSpaceStatus.statusLabel; color: appViewModel.diskSpaceStatus.low ? "#f2c5ba" : "#9ce3b6"; font.pixelSize: 13; font.bold: true }
                            }
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 260
                    title: "Cache Maintenance"
                    subtitle: "Clean the cache directory and inspect the last cleanup summary."

                    Column {
                        anchors.fill: parent
                        spacing: 10

                        Button {
                            text: "Clear Cache"
                            onClicked: appViewModel.clearCache()
                        }

                        Text {
                            text: appViewModel.cacheCleanupSummary.message.length > 0 ? appViewModel.cacheCleanupSummary.message : "No cache cleanup has been run yet."
                            color: "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: "Before: " + appViewModel.cacheCleanupSummary.bytesBeforeDisplay + "  |  After: " + appViewModel.cacheCleanupSummary.bytesAfterDisplay + "  |  Freed: " + appViewModel.cacheCleanupSummary.bytesFreedDisplay
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        Text {
                            text: "Files: " + appViewModel.cacheCleanupSummary.filesRemoved + "  |  Directories: " + appViewModel.cacheCleanupSummary.directoriesRemoved + "  |  State: " + appViewModel.cacheCleanupSummary.statusLabel
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }
}
