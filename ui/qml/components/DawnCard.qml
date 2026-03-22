import QtQuick
import QtQuick.Controls

Rectangle {
    id: root
    property string title: ""
    property string subtitle: ""
    property color accent: "#66a3ff"
    default property alias content: body.data

    radius: 20
    color: Qt.rgba(0.09, 0.12, 0.17, 0.88)
    border.color: Qt.rgba(1, 1, 1, 0.08)
    border.width: 1

    Column {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        Column {
            spacing: 4

            Text {
                text: root.title
                color: "#f4f7fb"
                font.pixelSize: 22
                font.bold: true
            }

            Text {
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
