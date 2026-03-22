import QtQuick
import FluentUI 1.0

Rectangle {
    id: root
    property string title: ""
    property var value: ""
    property string hint: ""

    width: 220
    height: 110
    radius: 18
    color: Qt.rgba(0.11, 0.15, 0.2, 0.9)
    border.color: Qt.rgba(1, 1, 1, 0.06)
    border.width: 1

    Column {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 8

        FluText {
            text: root.title
            color: "#9eb0c7"
            font.pixelSize: 12
            letterSpacing: 0.6
            font.capitalization: Font.AllUppercase
        }

        FluText {
            text: String(root.value)
            color: "#f5f8fb"
            font.pixelSize: 28
            font.bold: true
        }

        FluText {
            text: root.hint
            color: "#7f91a8"
            font.pixelSize: 12
            wrapMode: Text.WordWrap
        }
    }
}
