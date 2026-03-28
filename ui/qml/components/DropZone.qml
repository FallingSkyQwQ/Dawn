import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0

Rectangle {
    id: root

    property bool active: false
    property bool dragHover: false
    property var supportedExtensions: [".jar", ".zip", ".mrpack"]
    property var onFilesDropped: function(fileUrls) {}

    signal filesDropped(var fileUrls)

    anchors.fill: parent
    color: "transparent"
    visible: active
    z: 9999

    // 半透明遮罩层
    Rectangle {
        id: overlay
        anchors.fill: parent
        color: Qt.rgba(0, 0, 0, dragHover ? 0.6 : 0.4)
        opacity: active ? 1 : 0

        Behavior on opacity {
            NumberAnimation { duration: 200 }
        }
    }

    // 拖拽区域容器
    Rectangle {
        id: dropContainer
        anchors.centerIn: parent
        width: Math.min(parent.width * 0.8, 600)
        height: Math.min(parent.height * 0.6, 400)
        radius: 24
        color: dragHover ? Qt.rgba(0.15, 0.35, 0.55, 0.95) : Qt.rgba(0.08, 0.11, 0.15, 0.98)
        border.color: dragHover ? Qt.rgba(0.48, 0.64, 0.98, 0.8) : Qt.rgba(1, 1, 1, 0.1)
        border.width: dragHover ? 3 : 2

        // 边框发光效果
        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            color: "transparent"
            border.color: dragHover ? Qt.rgba(0.48, 0.64, 0.98, 0.3) : "transparent"
            border.width: 8
            opacity: dragHover ? 1 : 0

            Behavior on opacity {
                NumberAnimation { duration: 150 }
            }
        }

        Behavior on border.color {
            ColorAnimation { duration: 150 }
        }

        // 内容区域
        Column {
            anchors.centerIn: parent
            spacing: 20
            width: parent.width - 80

            // 图标
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                width: 80
                height: 80
                radius: 20
                color: dragHover ? Qt.rgba(0.48, 0.64, 0.98, 0.2) : Qt.rgba(1, 1, 1, 0.05)

                FluIcon {
                    anchors.centerIn: parent
                    iconSource: FluentIcons.Download
                    iconSize: 40
                    color: dragHover ? "#66a3ff" : "#8ea0b7"
                }

                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
            }

            // 主标题
            FluText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: dragHover ? "释放以安装" : "拖拽文件到此处"
                color: dragHover ? "#66a3ff" : "#f4f7fb"
                font.pixelSize: 24
                font.bold: true

                Behavior on color {
                    ColorAnimation { duration: 150 }
                }
            }

            // 副标题
            FluText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "支持安装 Mod、资源包、光影包和整合包"
                color: "#8ea0b7"
                font.pixelSize: 14
            }

            // 支持的文件类型标签
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 10

                Repeater {
                    model: [".jar", ".zip", ".mrpack"]

                    delegate: Rectangle {
                        width: tagText.width + 20
                        height: 28
                        radius: 6
                        color: Qt.rgba(1, 1, 1, 0.08)

                        FluText {
                            id: tagText
                            anchors.centerIn: parent
                            text: modelData
                            color: "#93a4bb"
                            font.pixelSize: 12
                            font.family: "Consolas, monospace"
                        }
                    }
                }
            }

            // 提示信息
            FluText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: "文件将被自动识别并安装到选定的实例"
                color: "#6a7a8f"
                font.pixelSize: 12
            }
        }
    }

    // 拖拽检测区域
    DropArea {
        anchors.fill: parent

        onEntered: function(drag) {
            if (!drag.hasUrls) {
                drag.accepted = false
                return
            }

            // 检查是否有支持的文件
            var hasSupportedFile = false
            for (var i = 0; i < drag.urls.length; i++) {
                var url = drag.urls[i]
                var filePath = url.toString()
                for (var j = 0; j < root.supportedExtensions.length; j++) {
                    if (filePath.toLowerCase().endsWith(root.supportedExtensions[j])) {
                        hasSupportedFile = true
                        break
                    }
                }
                if (hasSupportedFile) break
            }

            if (hasSupportedFile) {
                drag.accepted = true
                root.dragHover = true
            } else {
                drag.accepted = false
            }
        }

        onExited: {
            root.dragHover = false
        }

        onDropped: function(drop) {
            root.dragHover = false

            if (!drop.hasUrls) {
                drop.accepted = false
                return
            }

            // 过滤支持的文件
            var supportedFiles = []
            for (var i = 0; i < drop.urls.length; i++) {
                var url = drop.urls[i]
                var filePath = url.toString()
                for (var j = 0; j < root.supportedExtensions.length; j++) {
                    if (filePath.toLowerCase().endsWith(root.supportedExtensions[j])) {
                        supportedFiles.push(url)
                        break
                    }
                }
            }

            if (supportedFiles.length > 0) {
                drop.accepted = true
                root.filesDropped(supportedFiles)
                if (root.onFilesDropped) {
                    root.onFilesDropped(supportedFiles)
                }
            } else {
                drop.accepted = false
            }
        }
    }

    // 点击关闭
    MouseArea {
        anchors.fill: parent
        enabled: root.active && !root.dragHover
        onClicked: {
            root.active = false
        }
    }

    // 键盘快捷键关闭
    Keys.onEscapePressed: {
        root.active = false
    }

    function show() {
        active = true
        focus = true
    }

    function hide() {
        active = false
        dragHover = false
    }
}
