import QtQuick
import QtQuick.Controls
import QtQuick.Window
import "shell"

ApplicationWindow {
    id: root
    width: 1440
    height: 920
    visible: true
    title: "Dawn"
    color: "#0c1016"

    background: Rectangle {
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#0c1016" }
            GradientStop { position: 0.55; color: "#111824" }
            GradientStop { position: 1.0; color: "#0a0d12" }
        }
    }

    DawnAppShell {
        anchors.fill: parent
        appViewModel: appViewModel
    }
}
