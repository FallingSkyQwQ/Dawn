import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel
    property bool pendingCacheClear: false

    function scheduleDateObject() {
        var text = appViewModel ? appViewModel.backupScheduleDate : ""
        if (!text || text.length === 0) {
            return new Date()
        }
        var parts = text.split("-")
        if (parts.length !== 3) {
            return new Date()
        }
        return new Date(parseInt(parts[0]), parseInt(parts[1]) - 1, parseInt(parts[2]), 0, 0, 0)
    }

    function scheduleTimeObject() {
        var text = appViewModel ? appViewModel.backupScheduleTime : ""
        var now = new Date()
        if (!text || text.length === 0) {
            return now
        }
        var parts = text.split(":")
        if (parts.length !== 2) {
            return now
        }
        now.setHours(parseInt(parts[0]))
        now.setMinutes(parseInt(parts[1]))
        now.setSeconds(0)
        now.setMilliseconds(0)
        return now
    }

    function backupStrategyLabel(strategy) {
        switch (strategy) {
        case "manual": return "Manual"
        case "before-launch": return "Before Launch"
        case "before-update": return "Before Update"
        case "scheduled": return "Scheduled"
        default: return "Before Update"
        }
    }

    FluContentDialog {
        id: clearCacheDialog
        title: "Clear Cache"
        message: "Delete all cached downloads and temporary files now?"
        buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
        negativeText: "Cancel"
        positiveText: "Clear"
        onPositiveClicked: {
            root.pendingCacheClear = true
            var ok = root.appViewModel.clearCache()
            root.pendingCacheClear = false
            var win = root.Window.window
            if (win && win.showSuccess && win.showError) {
                if (ok) {
                    win.showSuccess("Cache cleared", 2400, root.appViewModel.cacheCleanupSummary.bytesFreedDisplay + " freed")
                } else {
                    win.showError("Cache cleanup failed", 3200, root.appViewModel.cacheCleanupSummary.message)
                }
            }
        }
    }

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
                Layout.preferredHeight: 150
                title: "Settings"
                subtitle: "Theme, windowing, downloads, caching, Java, accounts, and backups."

                Column {
                    anchors.fill: parent
                    spacing: 8

                    FluText {
                        text: "The initial pass now exposes persistent usability controls instead of static copy."
                        color: "#dce5f0"
                        font.pixelSize: 14
                        wrapMode: Text.WordWrap
                    }

                    FluText {
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

                        FluToggleSwitch {
                            id: uiModeSwitch
                            text: appViewModel.uiMode === "advanced" ? "Advanced mode" : "Novice mode"
                            checked: appViewModel.uiMode === "advanced"
                            onCheckedChanged: appViewModel.setUiMode(checked ? "advanced" : "novice")

                            FluTooltip {
                                visible: uiModeSwitch.hovered
                                delay: 500
                                text: "Novice keeps defaults compact; Advanced reveals runtime and launch override controls."
                            }
                        }

                        FluText {
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

                        FluText {
                            text: appViewModel.firstLaunchCompleted ? "First launch is marked complete." : "First launch wizard is still active."
                            color: "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        FluFilledButton {
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

                            FluText {
                                text: "Threshold (GB)"
                                color: "#dce5f0"
                                font.pixelSize: 13
                            }

                            FluButton {
                                id: thresholdHintButton
                                text: "?"
                                implicitWidth: 30
                                implicitHeight: 28

                                FluTooltip {
                                    visible: thresholdHintButton.hovered
                                    delay: 500
                                    text: "When free disk space is below this value, Dawn surfaces warnings before heavy install or repair tasks."
                                }
                            }

                            FluSpinBox {
                                id: thresholdSpin
                                from: 0
                                to: 1024
                                value: appViewModel.lowDiskThresholdGb
                                editable: true
                                onValueModified: appViewModel.setLowDiskThresholdGb(value)
                            }
                        }

                        FluFrame {
                            width: parent.width
                            implicitHeight: 66
                            radius: 10
                            border.color: Qt.rgba(1, 1, 1, 0.12)
                            color: appViewModel.lowDiskWarning.length > 0 ? Qt.rgba(0.33, 0.18, 0.16, 0.45) : Qt.rgba(1, 1, 1, 0.03)

                            Column {
                                anchors.fill: parent
                                anchors.margins: 10
                                spacing: 4

                                FluText {
                                    text: appViewModel.lowDiskWarning.length > 0 ? appViewModel.lowDiskWarning : "No low disk warning at the moment."
                                    color: appViewModel.lowDiskWarning.length > 0 ? "#f2c5ba" : "#dce5f0"
                                    font.pixelSize: 13
                                    wrapMode: Text.WordWrap
                                }

                                FluText {
                                    text: "Probe path: " + appViewModel.diskSpaceStatus.path
                                    color: "#8ea0b7"
                                    font.pixelSize: 12
                                    wrapMode: Text.WordWrap
                                }
                            }
                        }

                        FluDivider {
                            width: parent.width
                        }

                        RowLayout {
                            width: parent.width
                            spacing: 12

                            Column {
                                Layout.fillWidth: true
                                spacing: 4
                                FluText { text: "Available"; color: "#8ea0b7"; font.pixelSize: 11 }
                                FluText { text: appViewModel.diskSpaceStatus.availableDisplay; color: "#f5f8fb"; font.pixelSize: 13 }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 4
                                FluText { text: "Threshold"; color: "#8ea0b7"; font.pixelSize: 11 }
                                FluText { text: appViewModel.diskSpaceStatus.thresholdDisplay; color: "#f5f8fb"; font.pixelSize: 13 }
                            }

                            Column {
                                Layout.fillWidth: true
                                spacing: 4
                                FluText { text: "State"; color: "#8ea0b7"; font.pixelSize: 11 }
                                FluText { text: appViewModel.diskSpaceStatus.statusLabel; color: appViewModel.diskSpaceStatus.low ? "#f2c5ba" : "#9ce3b6"; font.pixelSize: 13; font.bold: true }
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

                        FluFilledButton {
                            id: clearCacheButton
                            text: root.pendingCacheClear ? "Clearing..." : "Clear Cache"
                            enabled: !root.pendingCacheClear
                            onClicked: clearCacheDialog.open()

                            FluTooltip {
                                visible: clearCacheButton.hovered
                                delay: 500
                                text: "Removes cached artifacts only. Instance manifests, saves, and installed mods are kept."
                            }
                        }

                        FluText {
                            text: appViewModel.cacheCleanupSummary.message.length > 0 ? appViewModel.cacheCleanupSummary.message : "No cache cleanup has been run yet."
                            color: "#dce5f0"
                            font.pixelSize: 13
                            wrapMode: Text.WordWrap
                        }

                        FluText {
                            text: "Before: " + appViewModel.cacheCleanupSummary.bytesBeforeDisplay + "  |  After: " + appViewModel.cacheCleanupSummary.bytesAfterDisplay + "  |  Freed: " + appViewModel.cacheCleanupSummary.bytesFreedDisplay
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        FluText {
                            text: "Files: " + appViewModel.cacheCleanupSummary.filesRemoved + "  |  Directories: " + appViewModel.cacheCleanupSummary.directoriesRemoved + "  |  State: " + appViewModel.cacheCleanupSummary.statusLabel
                            color: "#8ea0b7"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }
                }

                DawnCard {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 270
                    title: "Backup Policy"
                    subtitle: "Define when snapshots are created and configure scheduled backup time."

                    Column {
                        anchors.fill: parent
                        spacing: 10

                        RowLayout {
                            width: parent.width
                            spacing: 10

                            FluText {
                                text: "Strategy"
                                color: "#dce5f0"
                                font.pixelSize: 13
                            }

                            FluComboBox {
                                id: backupStrategyBox
                                Layout.fillWidth: true
                                textRole: "label"
                                valueRole: "value"
                                model: [
                                    { "label": "Manual", "value": "manual" },
                                    { "label": "Before Launch", "value": "before-launch" },
                                    { "label": "Before Update", "value": "before-update" },
                                    { "label": "Scheduled", "value": "scheduled" }
                                ]
                                currentIndex: {
                                    var strategy = appViewModel ? appViewModel.backupStrategy : "before-update"
                                    for (var i = 0; i < model.length; ++i) {
                                        if (model[i].value === strategy) {
                                            return i
                                        }
                                    }
                                    return 2
                                }
                                onActivated: appViewModel.setBackupStrategy(currentValue)
                            }
                        }

                        RowLayout {
                            width: parent.width
                            spacing: 10
                            visible: appViewModel.backupStrategy === "scheduled"

                            FluDatePicker {
                                id: backupDatePicker
                                current: root.scheduleDateObject()
                                onAccepted: appViewModel.setBackupScheduleDate(Qt.formatDate(current, "yyyy-MM-dd"))
                            }

                            FluTimePicker {
                                id: backupTimePicker
                                hourFormat: FluTimePickerType.HH
                                current: root.scheduleTimeObject()
                                onAccepted: appViewModel.setBackupScheduleTime(Qt.formatTime(current, "HH:mm"))
                            }
                        }

                        FluText {
                            text: appViewModel.backupStrategy === "scheduled"
                                  ? ("Next schedule: " + (appViewModel.backupScheduleDate.length > 0 ? appViewModel.backupScheduleDate : "not set")
                                     + " " + (appViewModel.backupScheduleTime.length > 0 ? appViewModel.backupScheduleTime : "not set"))
                                  : ("Current policy: " + root.backupStrategyLabel(appViewModel.backupStrategy))
                            color: "#dce5f0"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }

                        FluText {
                            text: "Scheduled mode uses persisted date/time in global settings and is shared across platforms."
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
