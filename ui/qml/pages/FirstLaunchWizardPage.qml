import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel
    property string onboardingVideoUrl: "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ForBiggerFun.mp4"

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
                Layout.preferredHeight: 160
                title: "First Launch Wizard"
                subtitle: "Complete the onboarding flow before entering the main shell."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    FluText {
                        text: "Welcome to Dawn. This wizard covers the initial setup path in four steps."
                        color: "#f5f8fb"
                        font.pixelSize: 17
                        font.bold: true
                        wrapMode: Text.WordWrap
                    }

                    FluText {
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

                        delegate: FluFrame {
                            Layout.fillWidth: true
                            height: 72
                            radius: 14
                            color: modelData.active ? Qt.rgba(0.26, 0.37, 0.55, 0.85) : Qt.rgba(1, 1, 1, 0.03)
                            border.color: modelData.completed ? Qt.rgba(0.48, 0.64, 0.98, 0.45) : Qt.rgba(1, 1, 1, 0.05)

                            Column {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 2

                                FluText {
                                    text: (modelData.index + 1) + ". " + modelData.title
                                    color: "#f5f8fb"
                                    font.pixelSize: 13
                                    font.bold: true
                                }

                                FluText {
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

                            RowLayout {
                                width: parent.width
                                spacing: 14

                                ColumnLayout {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 380
                                    spacing: 10

                                    FluText {
                                        text: "Dawn keeps instance state, content installs, and repair actions in separate workflows."
                                        color: "#dce5f0"
                                        font.pixelSize: 14
                                        wrapMode: Text.WordWrap
                                    }

                                    FluText {
                                        text: "Welcome media uses Qt Multimedia because the current FluentUI snapshot does not ship FluMediaPlayer."
                                        color: "#8ea0b7"
                                        font.pixelSize: 12
                                        wrapMode: Text.WordWrap
                                    }

                                    FluTextBox {
                                        Layout.fillWidth: true
                                        text: root.onboardingVideoUrl
                                        placeholderText: "Paste MP4 URL or local file URI"
                                        onEditingFinished: root.onboardingVideoUrl = text
                                    }

                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        FluButton {
                                            text: "Sample A"
                                            onClicked: root.onboardingVideoUrl = "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/ForBiggerFun.mp4"
                                        }

                                        FluButton {
                                            text: "Sample B"
                                            onClicked: root.onboardingVideoUrl = "https://commondatastorage.googleapis.com/gtv-videos-bucket/sample/BigBuckBunny.mp4"
                                        }
                                    }
                                }

                                FluFrame {
                                    Layout.fillWidth: true
                                    Layout.preferredWidth: 470
                                    Layout.fillHeight: true
                                    radius: 12
                                    color: Qt.rgba(1, 1, 1, 0.02)
                                    border.color: Qt.rgba(1, 1, 1, 0.08)

                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 8

                                        Item {
                                            width: parent.width
                                            height: 170

                                            Rectangle {
                                                anchors.fill: parent
                                                radius: 8
                                                color: "#101722"
                                                border.color: Qt.rgba(1, 1, 1, 0.10)
                                                border.width: 1
                                            }

                                            VideoOutput {
                                                id: onboardingVideoOutput
                                                anchors.fill: parent
                                                fillMode: VideoOutput.PreserveAspectFit
                                            }
                                        }

                                        RowLayout {
                                            width: parent.width
                                            spacing: 8

                                            FluButton {
                                                text: onboardingPlayer.playbackState === MediaPlayer.PlayingState ? "Pause" : "Play"
                                                onClicked: {
                                                    if (onboardingPlayer.playbackState === MediaPlayer.PlayingState) {
                                                        onboardingPlayer.pause()
                                                    } else {
                                                        onboardingPlayer.play()
                                                    }
                                                }
                                            }

                                            FluButton {
                                                text: "Restart"
                                                onClicked: {
                                                    onboardingPlayer.stop()
                                                    onboardingPlayer.play()
                                                }
                                            }

                                            FluButton {
                                                text: onboardingAudioOutput.muted ? "Unmute" : "Mute"
                                                onClicked: onboardingAudioOutput.muted = !onboardingAudioOutput.muted
                                            }

                                            FluText {
                                                Layout.fillWidth: true
                                                horizontalAlignment: Text.AlignRight
                                                text: onboardingPlayer.mediaStatus === MediaPlayer.BufferedMedia ? "Buffered" : "Loading"
                                                color: "#8ea0b7"
                                                font.pixelSize: 11
                                            }
                                        }

                                        FluText {
                                            width: parent.width
                                            visible: onboardingPlayer.error !== MediaPlayer.NoError
                                            text: "Video error: " + onboardingPlayer.errorString
                                            color: "#f2c5ba"
                                            font.pixelSize: 11
                                            wrapMode: Text.WordWrap
                                        }
                                    }
                                }
                            }

                            RowLayout {
                                width: parent.width
                                Item { Layout.fillWidth: true }
                                FluFilledButton {
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

                            FluText {
                                text: "Data root: " + appViewModel.dataRoot
                                color: "#f5f8fb"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            FluText {
                                text: "Cache path: " + appViewModel.cachePath
                                color: "#f5f8fb"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            FluText {
                                text: "Disk state: " + appViewModel.diskSpaceStatus.statusLabel + " | Available " + appViewModel.diskSpaceStatus.availableDisplay + " | Threshold " + appViewModel.diskSpaceStatus.thresholdDisplay
                                color: appViewModel.diskSpaceStatus.low ? "#f2c5ba" : "#9ce3b6"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            FluText {
                                text: appViewModel.lowDiskWarning.length > 0 ? appViewModel.lowDiskWarning : "Storage is within the configured threshold."
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                width: parent.width
                                FluButton {
                                    text: "Back"
                                    onClicked: appViewModel.previousWizardStep()
                                }
                                Item { Layout.fillWidth: true }
                                FluFilledButton {
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

                            FluText {
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

                                    delegate: FluButton {
                                        text: modelData.title
                                        checkable: true
                                        checked: appViewModel.javaStrategy === modelData.id
                                        onClicked: appViewModel.setJavaStrategy(modelData.id)
                                    }
                                }
                            }

                            FluText {
                                text: "Auto is the safest default. Bundled and custom paths are useful when you already manage a runtime."
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                width: parent.width
                                FluButton {
                                    text: "Back"
                                    onClicked: appViewModel.previousWizardStep()
                                }
                                Item { Layout.fillWidth: true }
                                FluFilledButton {
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

                            FluText {
                                text: "First launch completion will hide this wizard on the next route update."
                                color: "#dce5f0"
                                font.pixelSize: 14
                                wrapMode: Text.WordWrap
                            }

                            FluText {
                                text: "Data root: " + appViewModel.dataRoot + "\nCache path: " + appViewModel.cachePath + "\nJava strategy: " + appViewModel.javaStrategy
                                color: "#8ea0b7"
                                font.pixelSize: 12
                                wrapMode: Text.WordWrap
                            }

                            RowLayout {
                                width: parent.width
                                FluButton {
                                    text: "Back"
                                    onClicked: appViewModel.previousWizardStep()
                                }
                                Item { Layout.fillWidth: true }
                                FluFilledButton {
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

    AudioOutput {
        id: onboardingAudioOutput
        volume: 0.22
    }

    MediaPlayer {
        id: onboardingPlayer
        source: root.onboardingVideoUrl
        audioOutput: onboardingAudioOutput
        videoOutput: onboardingVideoOutput
    }
}
