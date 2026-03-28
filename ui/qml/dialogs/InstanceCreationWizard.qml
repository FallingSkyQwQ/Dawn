import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

FluContentDialog {
    id: root

    property var appViewModel
    property int currentStep: 1
    property int totalSteps: 4

    // Animation durations
    readonly property int microInteractionDuration: 140  // 120-160ms
    readonly property int panelSwitchDuration: 200  // 180-220ms
    readonly property int pageTransitionDuration: 260  // 240-300ms

    // Track previous step for animation direction
    property int previousStep: 1

    // Wizard data
    property string selectedBaseType: "vanilla"
    property string selectedVersion: ""
    property string selectedVersionType: "release"
    property string selectedLoader: "none"
    property string selectedLoaderVersion: ""
    property string instanceName: ""
    property string instanceIcon: "default"
    property int memoryAllocation: 4096
    property string selectedJavaProfile: "auto"
    property bool enablePerformancePack: true
    property bool enableAutoBackup: false

    title: "创建新实例"
    message: ""
    buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
    negativeText: "取消"
    positiveText: "完成"

    width: 800
    height: 650

    // Custom content delegate
    contentDelegate: Component {
        Item {
            implicitWidth: parent.width
            implicitHeight: contentColumn.height + 20

            Column {
                id: contentColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 20
                spacing: 20

                // Step indicator
                WizardStepIndicator {
                    width: parent.width
                    currentStep: root.currentStep
                    totalSteps: root.totalSteps
                    stepTitles: ["选择基础", "选择版本", "选择加载器", "实例预设"]
                }

                // Step 1: Base Selection
                Rectangle {
                    id: step1Rect
                    visible: root.currentStep === 1
                    width: parent.width
                    height: 380
                    radius: 12
                    color: Qt.rgba(0.08, 0.11, 0.15, 0.6)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    // Entry animation
                    opacity: 0
                    x: root.currentStep > root.previousStep ? 50 : -50

                    Behavior on opacity {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on x {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Component.onCompleted: {
                        opacity = 1
                        x = 0
                    }

                    onVisibleChanged: {
                        if (visible) {
                            opacity = 0
                            x = root.currentStep > root.previousStep ? 50 : -50
                            stepEntryAnimation.start()
                        }
                    }

                    SequentialAnimation {
                        id: stepEntryAnimation
                        ParallelAnimation {
                            NumberAnimation {
                                target: step1Rect
                                property: "opacity"
                                to: 1
                                duration: root.pageTransitionDuration
                                easing.type: Easing.OutCubic
                            }
                            NumberAnimation {
                                target: step1Rect
                                property: "x"
                                to: 0
                                duration: root.pageTransitionDuration
                                easing.type: Easing.OutCubic
                            }
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 16

                        FluText {
                            text: "选择实例基础类型"
                            color: "#f4f7fb"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        GridLayout {
                            Layout.fillWidth: true
                            columns: 2
                            columnSpacing: 16
                            rowSpacing: 16

                            Repeater {
                                model: [
                                    { id: "vanilla", name: "原版 Minecraft", desc: "纯净游戏体验，无 Mod 支持", icon: FluentIcons.Game, color: "#7dd87d" },
                                    { id: "modrinth", name: "Modrinth 整合包", desc: "从 Modrinth 下载并安装整合包", icon: FluentIcons.Download, color: "#8e9af7" },
                                    { id: "local", name: "本地整合包导入", desc: "导入已有的 .mrpack 或 ZIP 整合包", icon: FluentIcons.FolderOpen, color: "#e6a86e" },
                                    { id: "clone", name: "复制现有实例", desc: "基于已有实例创建副本", icon: FluentIcons.Copy, color: "#e67eb8" }
                                ]

                                delegate: Rectangle {
                                    id: baseTypeCard
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 100
                                    radius: 10
                                    color: modelData.id === root.selectedBaseType ?
                                           Qt.rgba(0.3, 0.5, 0.8, 0.4) : Qt.rgba(1, 1, 1, 0.05)
                                    border.color: modelData.id === root.selectedBaseType ?
                                                  Qt.rgba(0.5, 0.7, 1, 0.6) : Qt.rgba(1, 1, 1, 0.1)
                                    border.width: modelData.id === root.selectedBaseType ? 2 : 1

                                    // Hover animation
                                    scale: baseTypeMouseArea.containsMouse ? 1.02 : 1.0
                                    Behavior on scale {
                                        NumberAnimation {
                                            duration: root.microInteractionDuration
                                            easing.type: Easing.OutCubic
                                        }
                                    }

                                    // Selection animation
                                    Behavior on color {
                                        ColorAnimation {
                                            duration: root.microInteractionDuration
                                        }
                                    }

                                    Behavior on border.color {
                                        ColorAnimation {
                                            duration: root.microInteractionDuration
                                        }
                                    }

                                    // Entry animation with stagger
                                    opacity: 0
                                    y: 20
                                    Component.onCompleted: {
                                        baseCardEntryAnimation.start()
                                    }

                                    SequentialAnimation {
                                        id: baseCardEntryAnimation
                                        PauseAnimation { duration: index * 60 }
                                        ParallelAnimation {
                                            NumberAnimation {
                                                target: baseTypeCard
                                                property: "opacity"
                                                to: 1
                                                duration: 250
                                                easing.type: Easing.OutCubic
                                            }
                                            NumberAnimation {
                                                target: baseTypeCard
                                                property: "y"
                                                to: 0
                                                duration: 250
                                                easing.type: Easing.OutCubic
                                            }
                                        }
                                    }

                                    MouseArea {
                                        id: baseTypeMouseArea
                                        anchors.fill: parent
                                        hoverEnabled: true
                                        onClicked: root.selectedBaseType = modelData.id
                                    }

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 16
                                        spacing: 16

                                        Rectangle {
                                            Layout.preferredWidth: 48
                                            Layout.preferredHeight: 48
                                            radius: 10
                                            color: Qt.rgba(1, 1, 1, 0.1)

                                            FluIcon {
                                                anchors.centerIn: parent
                                                iconSource: modelData.icon
                                                iconSize: 24
                                                iconColor: modelData.color
                                            }
                                        }

                                        ColumnLayout {
                                            spacing: 4
                                            Layout.fillWidth: true

                                            FluText {
                                                text: modelData.name
                                                color: "#f5f8fb"
                                                font.pixelSize: 14
                                                font.bold: true
                                            }

                                            FluText {
                                                text: modelData.desc
                                                color: "#8ea0b7"
                                                font.pixelSize: 12
                                                wrapMode: Text.WordWrap
                                                Layout.fillWidth: true
                                            }
                                        }

                                        FluRadioButton {
                                            checked: modelData.id === root.selectedBaseType
                                            enabled: false
                                        }
                                    }
                                }
                            }
                        }

                        // Import/Clone specific options
                        Rectangle {
                            visible: root.selectedBaseType === "local"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            radius: 8
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.08)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                FluText {
                                    text: "整合包文件:"
                                    color: "#8ea0b7"
                                    font.pixelSize: 13
                                }

                                FluText {
                                    text: "点击选择文件..."
                                    color: "#66a3ff"
                                    font.pixelSize: 13
                                }

                                Item { Layout.fillWidth: true }

                                FluButton {
                                    text: "浏览"
                                    onClicked: {
                                        // Open file dialog
                                    }
                                }
                            }
                        }

                        Rectangle {
                            visible: root.selectedBaseType === "clone"
                            Layout.fillWidth: true
                            Layout.preferredHeight: 60
                            radius: 8
                            color: Qt.rgba(1, 1, 1, 0.03)
                            border.color: Qt.rgba(1, 1, 1, 0.08)

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 12
                                spacing: 12

                                FluText {
                                    text: "源实例:"
                                    color: "#8ea0b7"
                                    font.pixelSize: 13
                                }

                                FluComboBox {
                                    Layout.preferredWidth: 300
                                    model: root.getInstanceListModel()
                                }
                            }
                        }
                    }
                }

                // Step 2: Version Selection
                Rectangle {
                    id: step2Rect
                    visible: root.currentStep === 2
                    width: parent.width
                    height: 420
                    radius: 12
                    color: Qt.rgba(0.08, 0.11, 0.15, 0.6)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    // Entry animation
                    opacity: visible ? 1 : 0
                    x: visible ? 0 : (root.currentStep > root.previousStep ? 50 : -50)

                    Behavior on opacity {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on x {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        FluText {
                            text: "选择 Minecraft 版本"
                            color: "#f4f7fb"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        VersionSelector {
                            id: versionSelector
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            selectedVersion: root.selectedVersion
                            onVersionSelected: function(version, versionType) {
                                root.selectedVersion = version
                                root.selectedVersionType = versionType
                            }
                        }
                    }
                }

                // Step 3: Loader Selection
                Rectangle {
                    id: step3Rect
                    visible: root.currentStep === 3
                    width: parent.width
                    height: 420
                    radius: 12
                    color: Qt.rgba(0.08, 0.11, 0.15, 0.6)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    // Entry animation
                    opacity: visible ? 1 : 0
                    x: visible ? 0 : (root.currentStep > root.previousStep ? 50 : -50)

                    Behavior on opacity {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on x {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    ColumnLayout {
                        anchors.fill: parent
                        anchors.margins: 20
                        spacing: 12

                        FluText {
                            text: "选择 Mod 加载器"
                            color: "#f4f7fb"
                            font.pixelSize: 16
                            font.bold: true
                        }

                        LoaderSelector {
                            id: loaderSelector
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            mcVersion: root.selectedVersion
                            selectedLoader: root.selectedLoader
                            selectedLoaderVersion: root.selectedLoaderVersion
                            onLoaderSelected: function(loader, loaderVersion) {
                                root.selectedLoader = loader
                                root.selectedLoaderVersion = loaderVersion
                            }
                        }
                    }
                }

                // Step 4: Instance Presets
                Rectangle {
                    id: step4Rect
                    visible: root.currentStep === 4
                    width: parent.width
                    height: 420
                    radius: 12
                    color: Qt.rgba(0.08, 0.11, 0.15, 0.6)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    // Entry animation
                    opacity: visible ? 1 : 0
                    x: visible ? 0 : (root.currentStep > root.previousStep ? 50 : -50)

                    Behavior on opacity {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Behavior on x {
                        NumberAnimation {
                            duration: root.pageTransitionDuration
                            easing.type: Easing.OutCubic
                        }
                    }

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 20
                        contentWidth: parent.width - 40
                        contentHeight: presetColumn.height
                        clip: true
                        ScrollBar.vertical: FluScrollBar {}

                        Column {
                            id: presetColumn
                            width: parent.width
                            spacing: 16

                            FluText {
                                text: "配置实例预设"
                                color: "#f4f7fb"
                                font.pixelSize: 16
                                font.bold: true
                            }

                            // Instance name
                            RowLayout {
                                width: parent.width
                                spacing: 12

                                FluText {
                                    text: "实例名称:"
                                    color: "#dce5f0"
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 100
                                }

                                FluTextBox {
                                    id: nameTextBox
                                    Layout.fillWidth: true
                                    placeholderText: "输入实例名称..."
                                    text: root.instanceName
                                    onTextChanged: root.instanceName = text
                                }
                            }

                            // Icon selection
                            RowLayout {
                                width: parent.width
                                spacing: 12

                                FluText {
                                    text: "实例图标:"
                                    color: "#dce5f0"
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 100
                                }

                                Row {
                                    spacing: 8

                                    Repeater {
                                        model: ["default", "grass", "diamond", "creeper", "enderman"]

                                        delegate: Rectangle {
                                            width: 44
                                            height: 44
                                            radius: 8
                                            color: modelData === root.instanceIcon ?
                                                   Qt.rgba(0.4, 0.6, 1, 0.4) : Qt.rgba(1, 1, 1, 0.1)
                                            border.color: modelData === root.instanceIcon ?
                                                          Qt.rgba(0.6, 0.8, 1, 0.8) : "transparent"

                                            MouseArea {
                                                anchors.fill: parent
                                                onClicked: root.instanceIcon = modelData
                                            }

                                            FluIcon {
                                                anchors.centerIn: parent
                                                iconSource: FluentIcons.Game
                                                iconSize: 24
                                                iconColor: {
                                                    switch(modelData) {
                                                        case "grass": return "#7dd87d"
                                                        case "diamond": return "#7de0e6"
                                                        case "creeper": return "#7dd87d"
                                                        case "enderman": return "#b88ee6"
                                                        default: return "#66a3ff"
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Memory allocation
                            RowLayout {
                                width: parent.width
                                spacing: 12

                                FluText {
                                    text: "内存分配:"
                                    color: "#dce5f0"
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 100
                                }

                                FluSlider {
                                    id: memorySlider
                                    Layout.fillWidth: true
                                    from: 1024
                                    to: 16384
                                    stepSize: 512
                                    value: root.memoryAllocation
                                    onValueChanged: root.memoryAllocation = value
                                }

                                FluText {
                                    text: memorySlider.value + " MB"
                                    color: "#f5f8fb"
                                    font.pixelSize: 13
                                    font.bold: true
                                    Layout.preferredWidth: 80
                                }
                            }

                            // Java profile
                            RowLayout {
                                width: parent.width
                                spacing: 12

                                FluText {
                                    text: "Java 配置:"
                                    color: "#dce5f0"
                                    font.pixelSize: 13
                                    Layout.preferredWidth: 100
                                }

                                FluComboBox {
                                    Layout.preferredWidth: 200
                                    model: [
                                        { text: "自动选择", value: "auto" },
                                        { text: "Java 8", value: "java8" },
                                        { text: "Java 11", value: "java11" },
                                        { text: "Java 17", value: "java17" },
                                        { text: "Java 21", value: "java21" }
                                    ]
                                    textRole: "text"
                                    valueRole: "value"
                                    currentIndex: 0
                                    onActivated: root.selectedJavaProfile = currentValue
                                }

                                FluText {
                                    text: "推荐: " + root.getRecommendedJava()
                                    color: "#9ce3b6"
                                    font.pixelSize: 12
                                }
                            }

                            // Toggles
                            RowLayout {
                                width: parent.width
                                spacing: 24

                                FluToggleSwitch {
                                    text: "启用性能优化包"
                                    checked: root.enablePerformancePack
                                    onCheckedChanged: root.enablePerformancePack = checked
                                }

                                FluToggleSwitch {
                                    text: "启用自动备份"
                                    checked: root.enableAutoBackup
                                    onCheckedChanged: root.enableAutoBackup = checked
                                }
                            }

                            // Advanced settings expander
                            FluExpander {
                                width: parent.width
                                headerText: "高级设置"
                                contentHeight: advancedContent.height + 30

                                Column {
                                    id: advancedContent
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 15
                                    spacing: 12

                                    FluText {
                                        text: "JVM 参数 (可选)"
                                        color: "#8ea0b7"
                                        font.pixelSize: 12
                                    }

                                    FluTextBox {
                                        width: parent.width
                                        placeholderText: "-XX:+UseG1GC -XX:+UnlockExperimentalVMOptions..."
                                    }

                                    FluText {
                                        text: "环境变量 (可选)"
                                        color: "#8ea0b7"
                                        font.pixelSize: 12
                                    }

                                    FluTextBox {
                                        width: parent.width
                                        placeholderText: "KEY=value;KEY2=value2"
                                    }

                                    FluText {
                                        text: "游戏目录 (可选)"
                                        color: "#8ea0b7"
                                        font.pixelSize: 12
                                    }

                                    FluTextBox {
                                        width: parent.width
                                        placeholderText: "留空使用默认目录"
                                    }
                                }
                            }

                            // Summary
                            Rectangle {
                                width: parent.width
                                height: summaryColumn.height + 24
                                radius: 8
                                color: Qt.rgba(0.2, 0.4, 0.7, 0.3)
                                border.color: Qt.rgba(0.4, 0.6, 1, 0.4)

                                Column {
                                    id: summaryColumn
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 12
                                    spacing: 6

                                    FluText {
                                        text: "创建预览"
                                        color: "#ffffff"
                                        font.pixelSize: 13
                                        font.bold: true
                                    }

                                    FluText {
                                        text: "名称: " + (root.instanceName.length > 0 ? root.instanceName : "未命名实例")
                                        color: "#dce5f0"
                                        font.pixelSize: 12
                                    }

                                    FluText {
                                        text: "版本: Minecraft " + (root.selectedVersion.length > 0 ? root.selectedVersion : "未选择")
                                        color: "#dce5f0"
                                        font.pixelSize: 12
                                    }

                                    FluText {
                                        text: "加载器: " + (root.selectedLoader !== "none" ? root.selectedLoader + " " + root.selectedLoaderVersion : "无 (原版)")
                                        color: "#dce5f0"
                                        font.pixelSize: 12
                                    }

                                    FluText {
                                        text: "内存: " + root.memoryAllocation + " MB"
                                        color: "#dce5f0"
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    // Navigation buttons
    footer: Item {
        height: 60

        RowLayout {
            anchors.fill: parent
            anchors.margins: 20
            spacing: 12

            FluButton {
                id: prevBtn
                text: "上一步"
                enabled: root.currentStep > 1
                onClicked: {
                    if (root.currentStep > 1) {
                        root.previousStep = root.currentStep
                        root.currentStep--
                    }
                }

                // Hover animation
                scale: hovered ? 1.05 : 1.0
                Behavior on scale {
                    NumberAnimation {
                        duration: root.microInteractionDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            Item { Layout.fillWidth: true }

            FluButton {
                id: nextBtn
                visible: root.currentStep < root.totalSteps
                text: "下一步"
                enabled: root.canProceed()
                onClicked: {
                    if (root.currentStep < root.totalSteps) {
                        root.previousStep = root.currentStep
                        root.currentStep++
                        // Auto-generate instance name on first step
                        if (root.currentStep === 4 && root.instanceName.length === 0) {
                            root.instanceName = root.generateDefaultName()
                        }
                    }
                }

                // Hover animation
                scale: hovered ? 1.05 : 1.0
                Behavior on scale {
                    NumberAnimation {
                        duration: root.microInteractionDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }

            FluFilledButton {
                id: createBtn
                visible: root.currentStep === root.totalSteps
                text: "创建实例"
                enabled: root.canFinish()
                onClicked: root.createInstance()

                // Hover animation
                scale: hovered ? 1.05 : 1.0
                Behavior on scale {
                    NumberAnimation {
                        duration: root.microInteractionDuration
                        easing.type: Easing.OutCubic
                    }
                }

                // Click animation
                onPressed: {
                    createClickAnimation.start()
                }

                SequentialAnimation {
                    id: createClickAnimation
                    NumberAnimation {
                        target: createBtn
                        property: "scale"
                        from: hovered ? 1.05 : 1.0
                        to: 0.95
                        duration: 50
                    }
                    NumberAnimation {
                        target: createBtn
                        property: "scale"
                        from: 0.95
                        to: hovered ? 1.05 : 1.0
                        duration: 100
                        easing.type: Easing.OutBack
                    }
                }
            }
        }
    }

    onPositiveClicked: {
        root.previousStep = root.currentStep
        if (root.currentStep < root.totalSteps) {
            root.currentStep++
        } else {
            root.createInstance()
        }
    }

    onNegativeClicked: {
        root.reset()
        root.close()
    }

    function canProceed() {
        switch (root.currentStep) {
            case 1: return true
            case 2: return root.selectedVersion.length > 0
            case 3: return true
            case 4: return true
            default: return false
        }
    }

    function canFinish() {
        return root.selectedVersion.length > 0 && root.instanceName.length > 0
    }

    function generateDefaultName() {
        var name = "Minecraft " + root.selectedVersion
        if (root.selectedLoader !== "none") {
            name += " " + root.selectedLoader.charAt(0).toUpperCase() + root.selectedLoader.slice(1)
        }
        return name
    }

    function getRecommendedJava() {
        if (!root.selectedVersion) return "自动"
        var version = root.selectedVersion
        if (version.startsWith("1.21") || version.startsWith("1.20.5")) return "Java 21"
        if (version.startsWith("1.20") || version.startsWith("1.19") || version.startsWith("1.18")) return "Java 17"
        if (version.startsWith("1.17")) return "Java 16"
        return "Java 8"
    }

    function getInstanceListModel() {
        var listModel = Qt.createQmlObject('import QtQuick; ListModel {}', root)

        if (!root.appViewModel || !root.appViewModel.instanceCards) {
            return listModel
        }

        var cards = root.appViewModel.instanceCards
        for (var i = 0; i < cards.length; i++) {
            var card = cards[i]
            listModel.append({
                text: (card.name || "Unnamed") + " (" + (card.mcVersion || "?") + ")"
            })
        }

        return listModel
    }

    function createInstance() {
        if (!root.appViewModel) {
            showError("无法连接到应用程序")
            return
        }

        var success = root.appViewModel.createInstance(
            root.instanceName,
            root.selectedVersion,
            root.selectedLoader
        )

        if (success) {
            showSuccess("实例创建成功: " + root.instanceName)
            root.reset()
            root.close()
        } else {
            showError("实例创建失败")
        }
    }

    function reset() {
        root.currentStep = 1
        root.selectedBaseType = "vanilla"
        root.selectedVersion = ""
        root.selectedVersionType = "release"
        root.selectedLoader = "none"
        root.selectedLoaderVersion = ""
        root.instanceName = ""
        root.instanceIcon = "default"
        root.memoryAllocation = 4096
        root.selectedJavaProfile = "auto"
        root.enablePerformancePack = true
        root.enableAutoBackup = false
    }

    function open() {
        root.reset()
        super.open()
    }
}
