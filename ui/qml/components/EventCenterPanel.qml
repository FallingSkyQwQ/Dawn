import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

DawnCard {
    id: root

    property var eventsModel: []
    property var selectedContext: ({})
    property string statusFilter: "all"
    property string sourceFilter: "all"
    property string typeFilter: "all"
    property string selectedEventId: ""
    property bool showTypeFilter: true
    property bool showSourceFilter: true
    property bool showStatusFilter: true
    property bool showOpenContextButton: true
    property string entriesLabel: ""
    property string emptyTitle: "No events yet."
    property string emptySubtitle: "Install, repair, and diagnostic history will appear here."

    signal eventActivated(string eventId)
    signal statusFilterRequested(string value)
    signal sourceFilterRequested(string value)
    signal typeFilterRequested(string value)
    signal openContextRequested()

    function optionIndex(value, options) {
        for (var i = 0; i < options.length; ++i) {
            if (options[i].value === value) {
                return i
            }
        }
        return 0
    }

    function selectedEvent() {
        return root.selectedContext || {}
    }

    function contextPage() {
        return selectedEvent().eventTargetPage || selectedEvent().pageHint || "logs"
    }

    function contextInstance() {
        return selectedEvent().eventTargetInstanceId || selectedEvent().instanceId || ""
    }

    function contextProject() {
        return selectedEvent().eventTargetProjectId || selectedEvent().projectId || ""
    }

    function eventCountLabel() {
        if (root.entriesLabel.length > 0) {
            return root.entriesLabel
        }
        return (root.eventsModel ? root.eventsModel.length : 0) + " entries"
    }

    function isSelectedEvent(eventId) {
        const context = selectedEvent()
        return eventId === root.selectedEventId || eventId === context.eventId
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 10

            FluComboBox {
                visible: root.showTypeFilter
                Layout.preferredWidth: 160
                textRole: "label"
                valueRole: "value"
                model: [
                    { "label": "All Types", "value": "all" },
                    { "label": "Install", "value": "install" },
                    { "label": "Download", "value": "download" },
                    { "label": "Repair", "value": "repair" },
                    { "label": "Diagnostic", "value": "diagnostic" }
                ]
                currentIndex: root.optionIndex(root.typeFilter, model)
                onActivated: root.typeFilterRequested(currentValue)
            }

            FluComboBox {
                visible: root.showStatusFilter
                Layout.preferredWidth: 150
                textRole: "label"
                valueRole: "value"
                model: [
                    { "label": "All", "value": "all" },
                    { "label": "Success", "value": "success" },
                    { "label": "Failure", "value": "failure" }
                ]
                currentIndex: root.optionIndex(root.statusFilter, model)
                onActivated: root.statusFilterRequested(currentValue)
            }

            FluComboBox {
                visible: root.showSourceFilter
                Layout.preferredWidth: 190
                textRole: "label"
                valueRole: "value"
                model: [
                    { "label": "All Sources", "value": "all" },
                    { "label": "Local Drop", "value": "local_drop" },
                    { "label": "Remote Content", "value": "remote_content" },
                    { "label": "Repair", "value": "repair" },
                    { "label": "Diagnostic", "value": "diagnostic" }
                ]
                currentIndex: root.optionIndex(root.sourceFilter, model)
                onActivated: root.sourceFilterRequested(currentValue)
            }

            FluButton {
                visible: root.showOpenContextButton
                text: "Open Context"
                enabled: (root.selectedEvent().eventId || "").length > 0
                onClicked: root.openContextRequested()
            }

            Item { Layout.fillWidth: true }

            FluText {
                text: root.eventCountLabel()
                color: "#8ea0b7"
                font.pixelSize: 11
            }
        }

        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 150
            clip: true
            spacing: 8
            model: root.eventsModel

            delegate: Rectangle {
                width: ListView.view.width
                height: 74
                radius: 12
                color: root.isSelectedEvent(modelData.eventId) ? Qt.rgba(0.24, 0.35, 0.52, 0.96) : (modelData.success ? Qt.rgba(0.14, 0.24, 0.18, 0.95) : Qt.rgba(0.28, 0.17, 0.16, 0.95))
                border.color: root.isSelectedEvent(modelData.eventId) ? Qt.rgba(0.48, 0.64, 0.98, 0.55) : Qt.rgba(1, 1, 1, 0.05)

                MouseArea {
                    anchors.fill: parent
                    onClicked: root.eventActivated(modelData.eventId)
                }

                Column {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 3
                    FluText {
                        text: modelData.time + "  |  " + modelData.eventType + "  |  " + modelData.sourceType + "  |  " + modelData.result
                        color: "#f5f8fb"
                        font.pixelSize: 12
                        font.bold: true
                    }
                    FluText {
                        text: "Target: " + (modelData.targetInstanceId || "none") + "  |  " + modelData.summary
                        color: "#dce5f0"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                    }
                }
            }

            footer: Item {
                visible: root.eventsModel.length === 0
                width: ListView.view.width
                height: 110

                Column {
                    anchors.centerIn: parent
                    spacing: 6

                    FluText {
                        text: root.emptyTitle
                        color: "#f5f8fb"
                        font.pixelSize: 16
                        font.bold: true
                    }

                    FluText {
                        text: root.emptySubtitle
                        color: "#8ea0b7"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 96
            radius: 12
            color: Qt.rgba(1, 1, 1, 0.03)
            border.color: Qt.rgba(1, 1, 1, 0.05)

            Column {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 4

                FluText {
                    text: (root.selectedEvent().eventId || "").length > 0
                        ? ("Selected: " + (root.selectedEvent().eventType || "event") + " -> " + root.contextPage())
                        : "Select an event to preview context."
                    color: "#f5f8fb"
                    font.pixelSize: 13
                    font.bold: true
                }

                FluText {
                    text: root.selectedEvent().summary || "No event selected."
                    color: "#dce5f0"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }

                FluText {
                    text: (root.contextInstance().length > 0)
                        ? ("Instance: " + root.contextInstance())
                        : ((root.contextProject().length > 0)
                            ? ("Project: " + root.contextProject() + "  |  Version: " + (root.selectedEvent().versionId || ""))
                            : "No target context available.")
                    color: "#8ea0b7"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
}
