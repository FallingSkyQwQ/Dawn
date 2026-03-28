import QtQuick
import FluentUI 1.0

Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    property color accent: "#66a3ff"
    default property alias content: body.data

    // Animation durations
    readonly property int microInteractionDuration: 140  // 120-160ms
    readonly property int panelSwitchDuration: 200  // 180-220ms

    radius: 20
    color: Qt.rgba(0.09, 0.12, 0.17, 0.88)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    // Hover state
    property bool isHovered: false
    property real hoverScale: 1.0
    property real hoverElevation: 0
    property color hoverBorderColor: Qt.rgba(1, 1, 1, 0.08)

    // Scale animation on hover
    scale: hoverScale
    Behavior on scale {
        NumberAnimation {
            duration: microInteractionDuration
            easing.type: Easing.OutCubic
        }
    }

    // Border color animation
    Behavior on border.color {
        ColorAnimation {
            duration: microInteractionDuration
        }
    }

    // Mouse area for hover detection
    MouseArea {
        id: cardMouseArea
        anchors.fill: parent
        hoverEnabled: true
        onEntered: {
            root.isHovered = true
            root.hoverScale = 1.01
            root.hoverElevation = 1
            root.hoverBorderColor = Qt.rgba(0.48, 0.64, 0.98, 0.3)
            root.border.color = root.hoverBorderColor
        }
        onExited: {
            root.isHovered = false
            root.hoverScale = 1.0
            root.hoverElevation = 0
            root.border.color = Qt.rgba(1, 1, 1, 0.08)
        }
        onPressed: {
            root.hoverScale = 0.99
        }
        onReleased: {
            root.hoverScale = root.isHovered ? 1.01 : 1.0
        }
    }

    // Shadow effect layer
    Rectangle {
        id: shadowLayer
        anchors.fill: parent
        radius: parent.radius
        color: "transparent"
        opacity: root.hoverElevation * 0.3
        z: -1

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: Qt.rgba(0, 0, 0, 0.4)
            anchors.margins: -4
        }

        Behavior on opacity {
            NumberAnimation {
                duration: microInteractionDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    // Click feedback animation
    Rectangle {
        id: clickFeedback
        anchors.fill: parent
        radius: parent.radius
        color: Qt.rgba(0.48, 0.64, 0.98, 0.1)
        opacity: 0

        Behavior on opacity {
            NumberAnimation {
                duration: 200
            }
        }
    }

    Connections {
        target: cardMouseArea
        function onPressedChanged() {
            clickFeedback.opacity = cardMouseArea.pressed ? 0.5 : 0
        }
    }

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Column {
            spacing: 4

            FluText {
                text: root.title
                color: "#f4f7fb"
                font.pixelSize: 22
                font.bold: true
            }

            FluText {
                visible: root.subtitle.length > 0
                text: root.subtitle
                color: "#8ea0b7"
                font.pixelSize: 13
                wrapMode: Text.WordWrap
            }
        }

        Item {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            height: Math.max(0, parent.height - 68)
        }
    }
}
