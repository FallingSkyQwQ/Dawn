pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import FluentUI 1.0
import "../components"

Item {
    id: root
    property var appViewModel
    property string lastShownAutoCreatedInstanceId: ""
    property int searchPageCurrent: 1
    property int searchItemsPerPage: 8
    property int versionPageCurrent: 1
    property int versionItemsPerPage: 6

    function findIndexById(list, id, roleName) {
        for (var i = 0; i < list.length; ++i) {
            if (list[i] && list[i][roleName] === id) {
                return i
            }
        }
        return list.length > 0 ? 0 : -1
    }

    function searchNow() {
        root.appViewModel.searchContent(searchField.text, projectTypeBox.currentValue)
    }

    function toDependencyTreeNodes(node) {
        if (!node || !node.id) {
            return []
        }
        var treeNode = {
            "title": node.id + ((node.versionId || "").length > 0 ? (" @ " + node.versionId) : ""),
            "status": node.status || "",
            "requirement": node.requirement || "",
            "message": node.message || "",
            "children": []
        }
        if (node.children) {
            for (var i = 0; i < node.children.length; ++i) {
                var childNodes = toDependencyTreeNodes(node.children[i])
                if (childNodes.length > 0) {
                    treeNode.children.push(childNodes[0])
                }
            }
        }
        return [treeNode]
    }

    function dependencyTreeData() {
        return toDependencyTreeNodes(root.appViewModel.installPreview.dependencyTree)
    }

    function dependencyNodeCount(node) {
        if (!node || !node.id) {
            return 0
        }
        var count = 1
        if (node.children) {
            for (var i = 0; i < node.children.length; ++i) {
                count += dependencyNodeCount(node.children[i])
            }
        }
        return count
    }

    function dependencyReadyCount(node) {
        if (!node || !node.id) {
            return 0
        }
        var ready = (node.status === "ready" || node.status === "installed" || node.status === "embedded") ? 1 : 0
        if (node.children) {
            for (var i = 0; i < node.children.length; ++i) {
                ready += dependencyReadyCount(node.children[i])
            }
        }
        return ready
    }

    function dependencyProgressValue() {
        var rootNode = root.appViewModel ? root.appViewModel.installPreview.dependencyTree : null
        var total = dependencyNodeCount(rootNode)
        if (total <= 0) {
            return 0
        }
        var ready = dependencyReadyCount(rootNode)
        var ratio = ready / total
        if (ratio < 0) {
            return 0
        }
        if (ratio > 1) {
            return 1
        }
        return ratio
    }

    function dependencyProgressText() {
        return Math.round(dependencyProgressValue() * 100) + "%"
    }

    function dependencyStatusColor(status) {
        switch (status) {
        case "ready": return "#4bd18f"
        case "installed": return "#4bd18f"
        case "missing": return "#ffb74d"
        case "conflict": return "#ff6b6b"
        case "blocked": return "#ff6b6b"
        case "optional": return "#8ea0b7"
        case "embedded": return "#7aa2ff"
        default: return "#dce5f0"
        }
    }

    function rowsForInstances() {
        return root.appViewModel.instanceCards
    }

    function searchResultRows() {
        var rows = []
        var data = root.appViewModel ? root.appViewModel.contentSearchResults : []
        for (var i = 0; i < data.length; ++i) {
            var item = data[i]
            rows.push({
                "_key": item.projectId || ("search-" + i),
                "projectId": item.projectId || "",
                "title": item.title || "",
                "author": item.author || "",
                "downloads": item.downloads || 0,
                "summary": item.summary || "",
                "projectType": item.projectType || "",
                "selected": item.selected ? "yes" : "",
                "action": searchResultsTable.customItem(comSearchSelectAction, { "projectId": item.projectId || "" })
            })
        }
        return rows
    }

    function pagedRows(rows, pageCurrent, itemsPerPage) {
        if (!rows || rows.length === 0) {
            return []
        }
        if (itemsPerPage <= 0) {
            return rows
        }
        var page = pageCurrent
        if (page < 1) {
            page = 1
        }
        var start = (page - 1) * itemsPerPage
        if (start >= rows.length) {
            return []
        }
        var end = Math.min(start + itemsPerPage, rows.length)
        return rows.slice(start, end)
    }

    function searchPageCount() {
        var total = root.appViewModel ? root.appViewModel.contentSearchResults.length : 0
        return total > 0 ? Math.ceil(total / searchItemsPerPage) : 0
    }

    function versionPageCount() {
        var total = root.appViewModel ? root.appViewModel.contentVersions.length : 0
        return total > 0 ? Math.ceil(total / versionItemsPerPage) : 0
    }

    function normalizePage(page, count) {
        if (count <= 0) {
            return 1
        }
        if (page < 1) {
            return 1
        }
        if (page > count) {
            return count
        }
        return page
    }

    function pagedSearchResultRows() {
        return pagedRows(searchResultRows(), searchPageCurrent, searchItemsPerPage)
    }

    function versionRows() {
        var rows = []
        var data = root.appViewModel ? root.appViewModel.contentVersions : []
        for (var i = 0; i < data.length; ++i) {
            var item = data[i]
            rows.push({
                "_key": item.versionId || ("version-" + i),
                "versionId": item.versionId || "",
                "name": (item.name && item.name.length > 0) ? item.name : (item.versionId || ""),
                "gameVersions": (item.gameVersions && item.gameVersions.length > 0) ? item.gameVersions.join(", ") : "Any",
                "loaders": (item.loaders && item.loaders.length > 0) ? item.loaders.join(", ") : "Any",
                "selected": item.selected ? "yes" : "",
                "action": versionsTable.customItem(comVersionSelectAction, { "versionId": item.versionId || "" })
            })
        }
        return rows
    }

    function pagedVersionRows() {
        return pagedRows(versionRows(), versionPageCurrent, versionItemsPerPage)
    }

    function showAutoCreatedInstanceInfoBar() {
        if (!root.appViewModel || !root.appViewModel.autoCreatedInstanceNoticeVisible) {
            return
        }
        var instanceId = root.appViewModel.autoCreatedInstanceId || ""
        if (instanceId.length === 0 || instanceId === root.lastShownAutoCreatedInstanceId) {
            return
        }
        var win = root.Window.window
        if (win && win.showSuccess) {
            win.showSuccess(root.appViewModel.autoCreatedInstanceNoticeText, 3600, "Target instance has been activated.")
        }
        root.lastShownAutoCreatedInstanceId = instanceId
        root.appViewModel.clearAutoCreatedInstanceNotice()
    }

    Connections {
        target: root.appViewModel
        function onDataChanged() {
            root.showAutoCreatedInstanceInfoBar()
            root.searchPageCurrent = root.normalizePage(root.searchPageCurrent, root.searchPageCount())
            root.versionPageCurrent = root.normalizePage(root.versionPageCurrent, root.versionPageCount())
            if (dependencyTreeView) {
                dependencyTreeView.dataSource = root.dependencyTreeData()
            }
        }
    }

    Component.onCompleted: {
        root.showAutoCreatedInstanceInfoBar()
    }

    Flickable {
        anchors.fill: parent
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        clip: true

        ColumnLayout {
            id: contentColumn
            width: parent.width
            spacing: 18

            DawnCard {
                Layout.fillWidth: true
                Layout.preferredHeight: 180
                title: "Content Center"
                subtitle: "Search content, choose a target instance, inspect dependencies, and preview a repair plan."

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 12

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        FluTextBox {
                            id: searchField
                            Layout.fillWidth: true
                            placeholderText: "Search mods, modpacks, resource packs, shaders"
                            onCommit: function() { root.searchNow() }
                        }

                        FluComboBox {
                            id: projectTypeBox
                            Layout.preferredWidth: 180
                            textRole: "label"
                            valueRole: "value"
                            model: [
                                { "label": "Mod", "value": "mod" },
                                { "label": "Modpack", "value": "modpack" },
                                { "label": "Resourcepack", "value": "resourcepack" },
                                { "label": "Shader", "value": "shader" }
                            ]
                            currentIndex: 0
                        }

                        FluFilledButton {
                            text: "Search"
                            onClicked: root.searchNow()
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        FluComboBox {
                            id: instanceBox
                            Layout.fillWidth: true
                            textRole: "name"
                            valueRole: "id"
                            model: root.rowsForInstances()
                            currentIndex: root.findIndexById(root.rowsForInstances(), root.appViewModel.selectedTargetInstanceId, "id")
                            onActivated: root.appViewModel.selectTargetInstance(currentValue)
                        }

                        FluButton {
                            text: "Refresh Preview"
                            onClicked: root.appViewModel.refreshInstallPreview()
                        }
                    }

                    FluText {
                        text: root.appViewModel.installPreviewStatus
                        color: root.appViewModel.installPreview.blocked ? "#ffb74d" : "#4bd18f"
                        font.pixelSize: 13
                        wrapMode: Text.WordWrap
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 10

                        FluFilledButton {
                            text: "Install Selected"
                            enabled: !root.appViewModel.installPreview.blocked && root.appViewModel.selectedContentProjectId.length > 0 && root.appViewModel.selectedContentVersionId.length > 0
                            onClicked: root.appViewModel.installSelectedContent()
                        }

                        FluText {
                            Layout.fillWidth: true
                            text: root.appViewModel.contentInstallStatus
                            color: "#dce5f0"
                            font.pixelSize: 12
                            wrapMode: Text.WordWrap
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12

                        FluProgressRing {
                            Layout.preferredWidth: 46
                            Layout.preferredHeight: 46
                            indeterminate: false
                            progressVisible: true
                            value: root.dependencyProgressValue()
                        }

                        Column {
                            Layout.fillWidth: true
                            spacing: 6

                            FluText {
                                text: "Dependency readiness: " + root.dependencyProgressText()
                                color: "#dce5f0"
                                font.pixelSize: 12
                            }

                            FluProgressBar {
                                width: parent.width
                                indeterminate: false
                                progressVisible: true
                                value: root.dependencyProgressValue()
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: 16

                ColumnLayout {
                    Layout.preferredWidth: parent.width * 0.38
                    Layout.fillHeight: true
                    spacing: 16

                    DawnCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 390
                        title: "Search Results"
                        subtitle: root.appViewModel.contentSearchResults.length > 0 ? "Select a result to load versions and preview." : "Run a search to populate results."

                        Component {
                            id: comSearchSelectAction
                            Item {
                                FluButton {
                                    anchors.centerIn: parent
                                    text: "Select"
                                    onClicked: {
                                        if (options && options.projectId) {
                                            root.appViewModel.selectSearchResult(options.projectId)
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            FluTableView {
                                id: searchResultsTable
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                columnSource: [
                                    { "title": "Sel", "dataIndex": "selected", "width": 50 },
                                    { "title": "Title", "dataIndex": "title", "width": 140 },
                                    { "title": "Author", "dataIndex": "author", "width": 90 },
                                    { "title": "Downloads", "dataIndex": "downloads", "width": 80 },
                                    { "title": "Type", "dataIndex": "projectType", "width": 70 },
                                    { "title": "Summary", "dataIndex": "summary", "width": 140 },
                                    { "title": "Action", "dataIndex": "action", "width": 90 }
                                ]
                                dataSource: root.pagedSearchResultRows()
                            }

                            FluPagination {
                                Layout.alignment: Qt.AlignHCenter
                                pageCurrent: root.searchPageCurrent
                                pageButtonCount: 5
                                itemCount: root.appViewModel ? root.appViewModel.contentSearchResults.length : 0
                                __itemPerPage: root.searchItemsPerPage
                                onRequestPage: function(page) {
                                    root.searchPageCurrent = page
                                }
                            }
                        }
                    }

                    DawnCard {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 330
                        title: "Versions"
                        subtitle: root.appViewModel.contentVersions.length > 0 ? "Pick a version for the selected project." : "No versions loaded yet."

                        Component {
                            id: comVersionSelectAction
                            Item {
                                FluButton {
                                    anchors.centerIn: parent
                                    text: "Use"
                                    onClicked: {
                                        if (options && options.versionId) {
                                            root.appViewModel.selectInstallVersion(options.versionId)
                                        }
                                    }
                                }
                            }
                        }

                        ColumnLayout {
                            anchors.fill: parent
                            spacing: 10

                            FluTableView {
                                id: versionsTable
                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                columnSource: [
                                    { "title": "Sel", "dataIndex": "selected", "width": 50 },
                                    { "title": "Name", "dataIndex": "name", "width": 130 },
                                    { "title": "Game Versions", "dataIndex": "gameVersions", "width": 110 },
                                    { "title": "Loaders", "dataIndex": "loaders", "width": 80 },
                                    { "title": "Version ID", "dataIndex": "versionId", "width": 100 },
                                    { "title": "Action", "dataIndex": "action", "width": 80 }
                                ]
                                dataSource: root.pagedVersionRows()
                            }

                            FluPagination {
                                Layout.alignment: Qt.AlignHCenter
                                pageCurrent: root.versionPageCurrent
                                pageButtonCount: 5
                                itemCount: root.appViewModel ? root.appViewModel.contentVersions.length : 0
                                __itemPerPage: root.versionItemsPerPage
                                onRequestPage: function(page) {
                                    root.versionPageCurrent = page
                                }
                            }
                        }
                    }
                }

                ColumnLayout {
                    Layout.preferredWidth: parent.width * 0.62
                    Layout.fillHeight: true
                    spacing: 16

                    EventCenterPanel {
                Layout.fillWidth: true
                Layout.preferredHeight: 360
                title: "Event Center"
                subtitle: "Unified history for local drops, remote content installs, and repairs."
                eventsModel: root.appViewModel.eventCenter
                selectedContext: root.appViewModel.selectedEventContext
                selectedEventId: root.appViewModel.selectedEventId
                statusFilter: root.appViewModel.installLogFilter
                sourceFilter: root.appViewModel.installLogSourceFilter
                typeFilter: root.appViewModel.eventCenterTypeFilter
                onEventActivated: function(eventId) { root.appViewModel.selectEvent(eventId) }
                onStatusFilterRequested: function(value) { root.appViewModel.setInstallLogFilter(value) }
                onSourceFilterRequested: function(value) { root.appViewModel.setInstallLogSourceFilter(value) }
                onTypeFilterRequested: function(value) { root.appViewModel.setEventCenterTypeFilter(value) }
                onOpenContextRequested: function() { root.appViewModel.navigateToEventContext() }
            }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 300
                            title: "Dependency Tree"
                            subtitle: "Structured blockers and dependencies."

                            FluTreeView {
                                id: dependencyTreeView
                                anchors.fill: parent
                                cellHeight: 42
                                depthPadding: 14
                                showLine: true
                                checkable: false
                                columnSource: [
                                    { "title": "Dependency", "dataIndex": "title", "width": 260 },
                                    { "title": "Status", "dataIndex": "status", "width": 100 },
                                    { "title": "Requirement", "dataIndex": "requirement", "width": 100 },
                                    { "title": "Message", "dataIndex": "message", "width": 260 }
                                ]
                                dataSource: root.dependencyTreeData()
                            }
                        }

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 300
                            title: "Version Suggestions"
                            subtitle: "Recommended alternatives for the target instance."

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Repeater {
                                    model: root.appViewModel.installPreview.versionSuggestions

                                    delegate: FluFrame {
                                        width: parent.width
                                        height: 76
                                        radius: 12
                                        color: modelData.recommended ? Qt.rgba(0.16, 0.24, 0.36, 0.95) : Qt.rgba(1, 1, 1, 0.03)
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        MouseArea {
                                            anchors.fill: parent
                                            onClicked: root.appViewModel.selectInstallVersion(modelData.versionId)
                                        }

                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 3
                                            FluText { text: modelData.name; color: "#f5f8fb"; font.pixelSize: 13; font.bold: true }
                                            FluText { text: modelData.reason; color: "#8ea0b7"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 16

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            title: "Repair Plan"
                            subtitle: root.appViewModel.installPreview.repairPlanAvailable ? "One-click repair plan is available." : "No repair plan required."

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Repeater {
                                    model: root.appViewModel.installPreview.repairPlan.steps

                                    delegate: FluFrame {
                                        width: parent.width
                                        height: 62
                                        radius: 12
                                        color: Qt.rgba(1, 1, 1, 0.03)
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 3
                                            FluText { text: modelData.title; color: "#f5f8fb"; font.pixelSize: 13; font.bold: true }
                                            FluText { text: modelData.detail.length > 0 ? modelData.detail : modelData.status; color: "#8ea0b7"; font.pixelSize: 11 }
                                        }
                                    }
                                }
                            }
                        }

                        DawnCard {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 260
                            title: "Diagnostics"
                            subtitle: "Blocking and informational messages from the preview."

                            Column {
                                anchors.fill: parent
                                spacing: 8

                                Repeater {
                                    model: root.appViewModel.installDiagnostics

                                    delegate: FluFrame {
                                        width: parent.width
                                        height: 62
                                        radius: 12
                                        color: Qt.rgba(1, 1, 1, 0.03)
                                        border.color: Qt.rgba(1, 1, 1, 0.05)

                                        Column {
                                            anchors.fill: parent
                                            anchors.margins: 10
                                            spacing: 3
                                            FluText { text: modelData.code + "  |  " + modelData.severity; color: "#f5f8fb"; font.pixelSize: 12; font.bold: true }
                                            FluText { text: modelData.message; color: "#8ea0b7"; font.pixelSize: 11; wrapMode: Text.WordWrap }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
