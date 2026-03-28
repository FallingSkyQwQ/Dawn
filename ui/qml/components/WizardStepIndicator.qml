import QtQuick
import QtQuick.Layouts
import FluentUI 1.0

Rectangle {
    id: root

    property int currentStep: 1
    property int totalSteps: 4
    property var stepTitles: ["选择基础", "选择版本", "选择加载器", "实例预设"]

    height: 60
    color: "transparent"

    RowLayout {
        anchors.centerIn: parent
        spacing: 0

        Repeater {
            model: root.totalSteps

            delegate: RowLayout {
                spacing: 0

                // Step circle and label
                ColumnLayout {
                    spacing: 6

                    Rectangle {
                        Layout.alignment: Qt.AlignHCenter
                        width: 32
                        height: 32
                        radius: 16
                        color: {
                            if (index + 1 < root.currentStep) {
                                return FluTheme.primaryColor
                            } else if (index + 1 === root.currentStep) {
                                return FluTheme.primaryColor
                            } else {
                                return Qt.rgba(1, 1, 1, 0.1)
                            }
                        }
                        border.color: index + 1 === root.currentStep ? Qt.rgba(1, 1, 1, 0.5) : "transparent"
                        border.width: index + 1 === root.currentStep ? 2 : 0

                        FluIcon {
                            anchors.centerIn: parent
                            visible: index + 1 < root.currentStep
                            iconSource: FluentIcons.Accept
                            iconSize: 16
                            iconColor: "#ffffff"
                        }

                        FluText {
                            anchors.centerIn: parent
                            visible: index + 1 >= root.currentStep
                            text: String(index + 1)
                            color: index + 1 === root.currentStep ? "#ffffff" : "#8ea0b7"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }

                    FluText {
                        Layout.alignment: Qt.AlignHCenter
                        text: root.stepTitles[index] || ("Step " + (index + 1))
                        color: {
                            if (index + 1 === root.currentStep) {
                                return "#ffffff"
                            } else if (index + 1 < root.currentStep) {
                                return "#dce5f0"
                            } else {
                                return "#8ea0b7"
                            }
                        }
                        font.pixelSize: 12
                        font.bold: index + 1 === root.currentStep
                    }
                }

                // Connector line
                Rectangle {
                    visible: index < root.totalSteps - 1
                    Layout.preferredWidth: 60
                    Layout.preferredHeight: 2
                    color: index + 1 < root.currentStep ? FluTheme.primaryColor : Qt.rgba(1, 1, 1, 0.1)
                }
            }
        }
    }
}
