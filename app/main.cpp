#include "dawn/core/model/content_types.h"
#include "dawn/core/auth/account_service.h"
#include "dawn/core/service/preflight_service.h"
#include "dawn/core/model/task_types.h"
#include "dawn/core/service/instance_service.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/core/settings/settings_service.h"

#include <filesystem>
#include <iostream>

#if defined(DAWN_ENABLE_QT)
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QString>
#include <QUrl>

#include "app_view_model.h"
#endif

namespace {

std::filesystem::path default_data_root() {
    return std::filesystem::current_path() / "dawn-data";
}

void print_headless_summary() {
    using namespace dawn::core;

    const auto root = default_data_root();
    InstanceService instances(root);
    SettingsService settings(root);
    AccountService accountService(root);
    const auto list = instances.list_instances();
    const auto globalSettings = settings.load();
    const auto accountList = accountService.accounts();

    std::cout << "Dawn headless mode\n";
    std::cout << "Data root: " << root.generic_string() << '\n';
    std::cout << "Stored instances: " << list.size() << '\n';
    std::cout << "Theme: " << globalSettings.theme << '\n';
    std::cout << "Download concurrency: " << globalSettings.downloadConcurrency << '\n';
    std::cout << "Stored accounts: " << accountList.size() << '\n';
    for (const auto& manifest : list) {
        std::cout << "- " << manifest.name << " [" << manifest.mcVersion << "]\n";
    }

    PreflightService preflight;
    InstanceManifest demo;
    demo.id = "demo-instance";
    demo.name = "Demo Instance";
    demo.mcVersion = "1.20.1";
    demo.gameDir = (root / "instances" / demo.id / "game").generic_string();
    const auto result = preflight.inspect(demo);
    std::cout << "Demo preflight ready: " << (result.ready ? "yes" : "no") << '\n';
}

} // namespace

int main(int argc, char* argv[]) {
#if defined(DAWN_ENABLE_QT)
    QGuiApplication app(argc, argv);

    const auto dataRoot = default_data_root();
    const auto dataRootText = dataRoot.generic_string();
    dawn::ui::AppViewModel viewModel(QString::fromStdString(dataRootText));

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("appViewModel", &viewModel);

    const QString qmlPath = QString::fromUtf8(DAWN_SOURCE_DIR) + "/ui/qml/main.qml";
    engine.load(QUrl::fromLocalFile(qmlPath));
    if (engine.rootObjects().isEmpty()) {
        return -1;
    }
    return app.exec();
#else
    (void)argc;
    (void)argv;
    print_headless_summary();
    return 0;
#endif
}
