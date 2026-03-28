import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

FluContentDialog {
    id: root

    property var fileUrls: []
    property var appViewModel
    property var detectedFiles: []
    property string selectedInstanceId: ""
    property bool analyzing: false
    property bool installing: false

    // 文件类型枚举
    readonly property int TypeUnknown: 0
    readonly property int TypeMod: 1
    readonly property int TypeResourcePack: 2
    readonly property int TypeShaderPack: 3
    readonly property int TypeModpack: 4

    title: "拖拽安装"
    message: ""
    buttonFlags: FluContentDialogType.NegativeButton | FluContentDialogType.PositiveButton
    negativeText: "取消"
    positiveText: "确认安装"

    // 自定义内容区域
    contentDelegate: Component {
        Item {
            implicitWidth: parent.width
            implicitHeight: contentColumn.height + 40

            Column {
                id: contentColumn
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 20
                spacing: 16

                // 分析中提示
                Rectangle {
                    visible: root.analyzing
                    width: parent.width
                    height: 100
                    color: "transparent"

                    Column {
                        anchors.centerIn: parent
                        spacing: 12

                        FluProgressRing {
                            anchors.horizontalCenter: parent.horizontalCenter
                            width: 40
                            height: 40
                        }

                        FluText {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "正在分析文件..."
                            color: "#8ea0b7"
                            font.pixelSize: 14
                        }
                    }
                }

                // 文件列表
                Rectangle {
                    visible: !root.analyzing && root.detectedFiles.length > 0
                    width: parent.width
                    height: fileListColumn.height + 24
                    radius: 12
                    color: Qt.rgba(0.08, 0.11, 0.15, 0.6)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Column {
                        id: fileListColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        spacing: 10

                        FluText {
                            text: "检测到的文件 (" + root.detectedFiles.length + ")"
                            color: "#f4f7fb"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        Repeater {
                            model: root.detectedFiles

                            delegate: Rectangle {
                                width: parent.width
                                height: fileRow.height + 16
                                radius: 8
                                color: Qt.rgba(1, 1, 1, 0.03)

                                Row {
                                    id: fileRow
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.margins: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    spacing: 12

                                    // 文件类型图标
                                    Rectangle {
                                        width: 36
                                        height: 36
                                        radius: 8
                                        color: {
                                            switch (modelData.type) {
                                                case root.TypeMod: return Qt.rgba(0.4, 0.7, 0.4, 0.3)
                                                case root.TypeResourcePack: return Qt.rgba(0.7, 0.5, 0.3, 0.3)
                                                case root.TypeShaderPack: return Qt.rgba(0.5, 0.5, 0.8, 0.3)
                                                case root.TypeModpack: return Qt.rgba(0.7, 0.4, 0.6, 0.3)
                                                default: return Qt.rgba(0.5, 0.5, 0.5, 0.3)
                                            }
                                        }

                                        FluIcon {
                                            anchors.centerIn: parent
                                            iconSource: {
                                                switch (modelData.type) {
                                                    case root.TypeMod: return FluentIcons.Puzzle
                                                    case root.TypeResourcePack: return FluentIcons.Picture
                                                    case root.TypeShaderPack: return FluentIcons.Lightbulb
                                                    case root.TypeModpack: return FluentIcons.Package
                                                    default: return FluentIcons.Page
                                                }
                                            }
                                            iconSize: 18
                                            color: {
                                                switch (modelData.type) {
                                                    case root.TypeMod: return "#7dd87d"
                                                    case root.TypeResourcePack: return "#e6a86e"
                                                    case root.TypeShaderPack: return "#8e9af7"
                                                    case root.TypeModpack: return "#e67eb8"
                                                    default: return "#a0a0a0"
                                                }
                                            }
                                        }
                                    }

                                    Column {
                                        width: parent.width - 60
                                        spacing: 2

                                        FluText {
                                            text: modelData.fileName
                                            color: "#f4f7fb"
                                            font.pixelSize: 13
                                            font.bold: true
                                            elide: Text.ElideMiddle
                                            width: parent.width
                                        }

                                        Row {
                                            spacing: 8

                                            FluText {
                                                text: modelData.typeName
                                                color: {
                                                    switch (modelData.type) {
                                                        case root.TypeMod: return "#7dd87d"
                                                        case root.TypeResourcePack: return "#e6a86e"
                                                        case root.TypeShaderPack: return "#8e9af7"
                                                        case root.TypeModpack: return "#e67eb8"
                                                        default: return "#a0a0a0"
                                                    }
                                                }
                                                font.pixelSize: 11
                                            }

                                            FluText {
                                                visible: modelData.modId && modelData.modId.length > 0
                                                text: "ID: " + modelData.modId
                                                color: "#6a7a8f"
                                                font.pixelSize: 11
                                                font.family: "Consolas, monospace"
                                            }

                                            FluText {
                                                visible: modelData.version && modelData.version.length > 0
                                                text: "v" + modelData.version
                                                color: "#6a7a8f"
                                                font.pixelSize: 11
                                            }
                                        }

                                        FluText {
                                            visible: modelData.description && modelData.description.length > 0
                                            text: modelData.description
                                            color: "#6a7a8f"
                                            font.pixelSize: 10
                                            elide: Text.ElideRight
                                            width: parent.width
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // 目标实例选择
                Rectangle {
                    visible: !root.analyzing
                    width: parent.width
                    height: instanceSelectColumn.height + 24
                    radius: 12
                    color: Qt.rgba(0.08, 0.11, 0.15, 0.6)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Column {
                        id: instanceSelectColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        spacing: 10

                        FluText {
                            text: "目标实例"
                            color: "#f4f7fb"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        FluComboBox {
                            id: instanceComboBox
                            width: parent.width
                            enabled: !root.installing

                            model: root.getInstanceModel()

                            onCurrentIndexChanged: {
                                if (model.count > 0 && currentIndex >= 0) {
                                    root.selectedInstanceId = model.get(currentIndex).id
                                }
                            }

                            Component.onCompleted: {
                                // 默认选择当前活动实例
                                if (root.appViewModel && root.appViewModel.activeInstanceId) {
                                    for (var i = 0; i < model.count; i++) {
                                        if (model.get(i).id === root.appViewModel.activeInstanceId) {
                                            currentIndex = i
                                            break
                                        }
                                    }
                                }
                            }
                        }

                        FluText {
                            visible: root.selectedInstanceId.length === 0
                            text: "请选择一个目标实例以继续安装"
                            color: "#e6a86e"
                            font.pixelSize: 12
                        }
                    }
                }

                // 安装预览
                Rectangle {
                    visible: !root.analyzing && root.detectedFiles.length > 0 && root.selectedInstanceId.length > 0
                    width: parent.width
                    height: previewColumn.height + 24
                    radius: 12
                    color: Qt.rgba(0.08, 0.11, 0.15, 0.6)
                    border.color: Qt.rgba(1, 1, 1, 0.08)
                    border.width: 1

                    Column {
                        id: previewColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        spacing: 10

                        FluText {
                            text: "安装预览"
                            color: "#f4f7fb"
                            font.pixelSize: 14
                            font.bold: true
                        }

                        Repeater {
                            model: root.getInstallPreview()

                            delegate: Row {
                                spacing: 8

                                FluIcon {
                                    iconSource: FluentIcons.Accept
                                    iconSize: 14
                                    color: "#7dd87d"
                                }

                                FluText {
                                    text: modelData.text
                                    color: "#c5d0df"
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                }

                // 警告信息
                Rectangle {
                    visible: !root.analyzing && root.hasConflicts()
                    width: parent.width
                    height: warningColumn.height + 24
                    radius: 12
                    color: Qt.rgba(0.9, 0.5, 0.3, 0.15)
                    border.color: Qt.rgba(0.9, 0.5, 0.3, 0.3)
                    border.width: 1

                    Column {
                        id: warningColumn
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.margins: 12
                        spacing: 8

                        Row {
                            spacing: 8

                            FluIcon {
                                iconSource: FluentIcons.Warning
                                iconSize: 16
                                color: "#e6a86e"
                            }

                            FluText {
                                text: "检测到冲突"
                                color: "#e6a86e"
                                font.pixelSize: 13
                                font.bold: true
                            }
                        }

                        Repeater {
                            model: root.getConflictList()

                            delegate: FluText {
                                text: "• " + modelData
                                color: "#d4a88c"
                                font.pixelSize: 11
                            }
                        }
                    }
                }
            }
        }
    }

    onPositiveClicked: {
        if (root.selectedInstanceId.length === 0) {
            showWarning("请选择一个目标实例")
            return
        }
        root.startInstall()
    }

    onNegativeClicked: {
        root.close()
    }

    function getInstanceModel() {
        var listModel = Qt.createQmlObject('import QtQuick; ListModel {}', root)

        if (!root.appViewModel || !root.appViewModel.instanceCards) {
            return listModel
        }

        var cards = root.appViewModel.instanceCards
        for (var i = 0; i < cards.length; i++) {
            var card = cards[i]
            listModel.append({
                id: card.id || "",
                text: (card.name || "Unnamed") + " (" + (card.mcVersion || "?") + ")"
            })
        }

        return listModel
    }

    function getInstallPreview() {
        var preview = []
        var modCount = 0
        var resourcePackCount = 0
        var shaderPackCount = 0
        var modpackCount = 0

        for (var i = 0; i < root.detectedFiles.length; i++) {
            var file = root.detectedFiles[i]
            switch (file.type) {
                case root.TypeMod: modCount++; break
                case root.TypeResourcePack: resourcePackCount++; break
                case root.TypeShaderPack: shaderPackCount++; break
                case root.TypeModpack: modpackCount++; break
            }
        }

        if (modCount > 0) {
            preview.push({ text: "安装 " + modCount + " 个 Mod 到 mods/ 文件夹" })
        }
        if (resourcePackCount > 0) {
            preview.push({ text: "安装 " + resourcePackCount + " 个资源包到 resourcepacks/ 文件夹" })
        }
        if (shaderPackCount > 0) {
            preview.push({ text: "安装 " + shaderPackCount + " 个光影包到 shaderpacks/ 文件夹" })
        }
        if (modpackCount > 0) {
            preview.push({ text: "安装 " + modpackCount + " 个整合包（可能需要覆盖现有文件）" })
        }

        return preview
    }

    function hasConflicts() {
        // 检查是否有同名文件冲突
        for (var i = 0; i < root.detectedFiles.length; i++) {
            var file = root.detectedFiles[i]
            if (file.exists) {
                return true
            }
        }
        return false
    }

    function getConflictList() {
        var conflicts = []
        for (var i = 0; i < root.detectedFiles.length; i++) {
            var file = root.detectedFiles[i]
            if (file.exists) {
                conflicts.push(file.fileName + " 已存在，将被覆盖")
            }
        }
        return conflicts
    }

    function analyzeFiles(urls) {
        root.fileUrls = urls
        root.analyzing = true
        root.detectedFiles = []

        // 模拟异步分析（实际实现中应该调用 C++ 后端）
        analyzeTimer.start()
    }

    Timer {
        id: analyzeTimer
        interval: 800
        onTriggered: {
            root.performAnalysis()
            root.analyzing = false
        }
    }

    function performAnalysis() {
        var results = []

        for (var i = 0; i < root.fileUrls.length; i++) {
            var url = root.fileUrls[i]
            var filePath = url.toString()
            var fileName = extractFileName(filePath)
            var typeInfo = detectFileType(filePath, fileName)

            results.push({
                url: url,
                filePath: filePath,
                fileName: fileName,
                type: typeInfo.type,
                typeName: typeInfo.typeName,
                modId: typeInfo.modId || "",
                version: typeInfo.version || "",
                description: typeInfo.description || "",
                exists: false // 实际应该检查目标位置是否存在
            })
        }

        root.detectedFiles = results
    }

    function extractFileName(filePath) {
        var parts = filePath.split("/")
        var name = parts[parts.length - 1]
        // 移除 file:// 前缀
        if (name.indexOf("file://") === 0) {
            name = name.substring(7)
        }
        // URL 解码
        try {
            name = decodeURIComponent(name)
        } catch (e) {
            // 解码失败，使用原始名称
        }
        return name
    }

    function detectFileType(filePath, fileName) {
        var lowerPath = filePath.toLowerCase()
        var lowerName = fileName.toLowerCase()

        // 检查扩展名
        if (lowerName.endsWith(".mrpack")) {
            return {
                type: root.TypeModpack,
                typeName: "整合包",
                description: "Modrinth 整合包格式"
            }
        }

        if (lowerName.endsWith(".jar")) {
            // 尝试从文件名识别 Mod 信息
            var modInfo = extractModInfoFromFileName(fileName)
            return {
                type: root.TypeMod,
                typeName: "Mod",
                modId: modInfo.id,
                version: modInfo.version,
                description: modInfo.description
            }
        }

        if (lowerName.endsWith(".zip")) {
            // 根据文件名或内容猜测类型
            if (lowerName.includes("shader") || lowerName.includes("光影")) {
                return {
                    type: root.TypeShaderPack,
                    typeName: "光影包",
                    description: "光影效果包"
                }
            }
            if (lowerName.includes("resource") || lowerName.includes("资源")) {
                return {
                    type: root.TypeResourcePack,
                    typeName: "资源包",
                    description: "材质资源包"
                }
            }
            if (lowerName.includes("modpack") || lowerName.includes("整合")) {
                return {
                    type: root.TypeModpack,
                    typeName: "整合包",
                    description: "游戏整合包"
                }
            }

            // 默认假设为资源包
            return {
                type: root.TypeResourcePack,
                typeName: "资源包",
                description: "ZIP 压缩包（需要进一步确认内容）"
            }
        }

        return {
            type: root.TypeUnknown,
            typeName: "未知",
            description: "无法识别的文件类型"
        }
    }

    function extractModInfoFromFileName(fileName) {
        // 尝试从文件名解析 Mod ID 和版本
        // 常见格式: modid-1.20.1-1.2.3.jar 或 modid-1.2.3.jar
        var baseName = fileName.replace(/\.jar$/i, "")
        var parts = baseName.split("-")

        var result = {
            id: "",
            version: "",
            description: ""
        }

        if (parts.length >= 2) {
            result.id = parts[0]
            // 尝试找到版本号（通常包含数字点号）
            for (var i = 1; i < parts.length; i++) {
                if (/\d/.test(parts[i])) {
                    result.version = parts[i]
                    break
                }
            }
        } else {
            result.id = baseName
        }

        return result
    }

    function startInstall() {
        if (root.selectedInstanceId.length === 0) {
            showWarning("请选择一个目标实例")
            return
        }

        root.installing = true

        // 调用 ViewModel 进行安装
        if (root.appViewModel) {
            var filePaths = []
            for (var i = 0; i < root.detectedFiles.length; i++) {
                filePaths.push(root.detectedFiles[i].filePath)
            }

            var success = root.appViewModel.installDroppedFiles(
                root.selectedInstanceId,
                filePaths,
                root.detectedFiles
            )

            if (success) {
                showSuccess("安装任务已创建")
                root.close()
            } else {
                showError("安装失败")
                root.installing = false
            }
        } else {
            showError("无法连接到应用程序")
            root.installing = false
        }
    }

    function reset() {
        root.fileUrls = []
        root.detectedFiles = []
        root.selectedInstanceId = ""
        root.analyzing = false
        root.installing = false
    }
}
