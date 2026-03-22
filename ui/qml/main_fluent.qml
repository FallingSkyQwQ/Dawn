import QtQuick 2.15
import QtQuick.Window 2.15
import FluentUI 1.0

FluWindow {
    id: window
    width: 1440
    height: 920
    minimumWidth: 1080
    minimumHeight: 720
    title: "Dawn"
    fitsAppBarWindows: true
    showDark: true

    property var dawnViewModel: appViewModel
    readonly property bool showWizard: dawnViewModel && dawnViewModel.firstLaunchVisible

    appBar: FluAppBar {
        title: "Dawn"
        showDark: true
        height: 32
    }

    Component.onCompleted: {
        FluTheme.animationEnabled = true
        nav.setCurrentIndex(showWizard ? 0 : 1)
    }

    onShowWizardChanged: {
        if (!showWizard && nav.getCurrentIndex() <= 0) {
            nav.setCurrentIndex(0)
        }
    }

    Connections {
        target: dawnViewModel
        function onNavigateToPageRequested(pageIndex) {
            if (pageIndex < 0) {
                return
            }
            const offset = showWizard ? 1 : 0
            nav.setCurrentIndex(pageIndex + offset)
        }
    }

    FluNavigationView {
        id: nav
        anchors.fill: parent
        title: "Dawn"
        pageMode: FluNavigationViewType.NoStack
        displayMode: FluNavigationViewType.Open
        topPadding: window.useSystemAppBar ? 0 : (FluTools.isMacos() ? 20 : 0)

        items: FluObject {
            FluPaneItem {
                visible: window.showWizard
                title: "首次向导"
                icon: FluentIcons.Home
                url: Qt.resolvedUrl("pages/FirstLaunchWizardPage.qml")
                onTap: nav.push(url, {"appViewModel": window.dawnViewModel})
            }
            FluPaneItem {
                title: "首页"
                icon: FluentIcons.Home
                url: Qt.resolvedUrl("pages/HomePage.qml")
                onTap: nav.push(url, {"appViewModel": window.dawnViewModel})
            }
            FluPaneItem {
                title: "实例"
                icon: FluentIcons.AllApps
                url: Qt.resolvedUrl("pages/InstancesPage.qml")
                onTap: nav.push(url, {"appViewModel": window.dawnViewModel})
            }
            FluPaneItem {
                title: "内容中心"
                icon: FluentIcons.Shop
                url: Qt.resolvedUrl("pages/ContentCenterPage.qml")
                onTap: nav.push(url, {"appViewModel": window.dawnViewModel})
            }
            FluPaneItem {
                title: "下载队列"
                icon: FluentIcons.AllApps
                url: Qt.resolvedUrl("pages/DownloadQueuePage.qml")
                onTap: nav.push(url, {"appViewModel": window.dawnViewModel})
            }
            FluPaneItem {
                title: "日志与修复"
                icon: FluentIcons.AllApps
                url: Qt.resolvedUrl("pages/LogsRepairPage.qml")
                onTap: nav.push(url, {"appViewModel": window.dawnViewModel})
            }
            FluPaneItem {
                title: "设置"
                icon: FluentIcons.Settings
                url: Qt.resolvedUrl("pages/SettingsPage.qml")
                onTap: nav.push(url, {"appViewModel": window.dawnViewModel})
            }
        }
    }
}
