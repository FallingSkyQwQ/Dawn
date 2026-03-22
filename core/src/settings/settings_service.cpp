#include "dawn/core/settings/settings_service.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;

std::string to_string(JavaStrategy strategy) {
    switch (strategy) {
    case JavaStrategy::Auto: return "auto";
    case JavaStrategy::Bundled: return "bundled";
    case JavaStrategy::CustomPath: return "custom-path";
    case JavaStrategy::Downloaded: return "downloaded";
    }
    return "auto";
}

std::string to_string(MemoryStrategy strategy) {
    switch (strategy) {
    case MemoryStrategy::Balanced: return "balanced";
    case MemoryStrategy::Conservative: return "conservative";
    case MemoryStrategy::Performance: return "performance";
    case MemoryStrategy::Custom: return "custom";
    }
    return "balanced";
}

std::string to_string(BackupStrategy strategy) {
    switch (strategy) {
    case BackupStrategy::Manual: return "manual";
    case BackupStrategy::BeforeLaunch: return "before-launch";
    case BackupStrategy::BeforeUpdate: return "before-update";
    case BackupStrategy::Scheduled: return "scheduled";
    }
    return "before-update";
}

JavaStrategy java_strategy_from_string(const std::string& text) {
    if (text == "bundled") return JavaStrategy::Bundled;
    if (text == "custom-path") return JavaStrategy::CustomPath;
    if (text == "downloaded") return JavaStrategy::Downloaded;
    return JavaStrategy::Auto;
}

MemoryStrategy memory_strategy_from_string(const std::string& text) {
    if (text == "conservative") return MemoryStrategy::Conservative;
    if (text == "performance") return MemoryStrategy::Performance;
    if (text == "custom") return MemoryStrategy::Custom;
    return MemoryStrategy::Balanced;
}

BackupStrategy backup_strategy_from_string(const std::string& text) {
    if (text == "manual") return BackupStrategy::Manual;
    if (text == "before-launch") return BackupStrategy::BeforeLaunch;
    if (text == "scheduled") return BackupStrategy::Scheduled;
    return BackupStrategy::BeforeUpdate;
}

Value global_settings_to_json(const GlobalSettings& settings) {
    Value::Object proxy;
    proxy.emplace("enabled", Value(settings.proxy.enabled));
    proxy.emplace("scheme", Value(settings.proxy.scheme));
    proxy.emplace("host", Value(settings.proxy.host));
    proxy.emplace("port", Value(settings.proxy.port));
    proxy.emplace("username", Value(settings.proxy.username));
    proxy.emplace("password", Value(settings.proxy.password));

    Value::Array flags;
    for (const auto& flag : settings.experimentalFlags) {
        flags.emplace_back(flag);
    }

    Value::Object object;
    object.emplace("theme", Value(settings.theme));
    object.emplace("downloadConcurrency", Value(settings.downloadConcurrency));
    object.emplace("cachePath", Value(settings.cachePath.generic_string()));
    object.emplace("javaStrategy", Value(to_string(settings.javaStrategy)));
    object.emplace("javaRuntimePath", Value(settings.javaRuntimePath));
    object.emplace("memoryStrategy", Value(to_string(settings.memoryStrategy)));
    object.emplace("minMemoryMb", Value(settings.minMemoryMb));
    object.emplace("maxMemoryMb", Value(settings.maxMemoryMb));
    object.emplace("backupStrategy", Value(to_string(settings.backupStrategy)));
    object.emplace("proxy", Value(std::move(proxy)));
    object.emplace("experimentalMode", Value(settings.experimentalMode));
    object.emplace("experimentalFlags", Value(std::move(flags)));
    return Value(std::move(object));
}

bool global_settings_from_json(const Value& value, GlobalSettings* settings, std::string* error) {
    if (!settings || !value.is_object()) {
        if (error) {
            *error = "global settings json must be an object";
        }
        return false;
    }

    const auto& object = value.as_object();
    const auto* theme = dawn::infra::json::find(object, "theme");
    const auto* concurrency = dawn::infra::json::find(object, "downloadConcurrency");
    const auto* cache_path = dawn::infra::json::find(object, "cachePath");
    const auto* java_strategy = dawn::infra::json::find(object, "javaStrategy");
    const auto* java_runtime_path = dawn::infra::json::find(object, "javaRuntimePath");
    const auto* memory_strategy = dawn::infra::json::find(object, "memoryStrategy");
    const auto* min_memory = dawn::infra::json::find(object, "minMemoryMb");
    const auto* max_memory = dawn::infra::json::find(object, "maxMemoryMb");
    const auto* backup_strategy = dawn::infra::json::find(object, "backupStrategy");
    const auto* proxy = dawn::infra::json::find(object, "proxy");
    const auto* experimental_mode = dawn::infra::json::find(object, "experimentalMode");
    const auto* experimental_flags = dawn::infra::json::find(object, "experimentalFlags");

    if (!theme || !theme->is_string() || !concurrency || !concurrency->is_number() ||
        !cache_path || !cache_path->is_string() || !java_strategy || !java_strategy->is_string() ||
        !java_runtime_path || !java_runtime_path->is_string() || !memory_strategy || !memory_strategy->is_string() ||
        !min_memory || !min_memory->is_number() || !max_memory || !max_memory->is_number() ||
        !backup_strategy || !backup_strategy->is_string()) {
        if (error) {
            *error = "missing or invalid global settings field";
        }
        return false;
    }

    settings->theme = theme->as_string();
    settings->downloadConcurrency = static_cast<int>(concurrency->as_number());
    settings->cachePath = std::filesystem::path(cache_path->as_string());
    settings->javaStrategy = java_strategy_from_string(java_strategy->as_string());
    settings->javaRuntimePath = java_runtime_path->as_string();
    settings->memoryStrategy = memory_strategy_from_string(memory_strategy->as_string());
    settings->minMemoryMb = static_cast<int>(min_memory->as_number());
    settings->maxMemoryMb = static_cast<int>(max_memory->as_number());
    settings->backupStrategy = backup_strategy_from_string(backup_strategy->as_string());

    settings->proxy = ProxySettings{};
    if (proxy && proxy->is_object()) {
        const auto& proxy_object = proxy->as_object();
        if (const auto* enabled = dawn::infra::json::find(proxy_object, "enabled"); enabled && enabled->is_bool()) settings->proxy.enabled = enabled->as_bool();
        if (const auto* scheme = dawn::infra::json::find(proxy_object, "scheme"); scheme && scheme->is_string()) settings->proxy.scheme = scheme->as_string();
        if (const auto* host = dawn::infra::json::find(proxy_object, "host"); host && host->is_string()) settings->proxy.host = host->as_string();
        if (const auto* port = dawn::infra::json::find(proxy_object, "port"); port && port->is_number()) settings->proxy.port = static_cast<int>(port->as_number());
        if (const auto* username = dawn::infra::json::find(proxy_object, "username"); username && username->is_string()) settings->proxy.username = username->as_string();
        if (const auto* password = dawn::infra::json::find(proxy_object, "password"); password && password->is_string()) settings->proxy.password = password->as_string();
    }

    settings->experimentalMode = experimental_mode && experimental_mode->is_bool() ? experimental_mode->as_bool() : false;
    settings->experimentalFlags.clear();
    if (experimental_flags && experimental_flags->is_array()) {
        for (const auto& flag : experimental_flags->as_array()) {
            if (flag.is_string()) {
                settings->experimentalFlags.push_back(flag.as_string());
            }
        }
    }
    return true;
}

} // namespace

SettingsService::SettingsService(std::filesystem::path root) : root_(std::move(root)) {}

const std::filesystem::path& SettingsService::root() const noexcept {
    return root_;
}

std::filesystem::path SettingsService::settings_path() const {
    return root_ / "settings" / "global-settings.json";
}

GlobalSettings SettingsService::load(std::string* error) const {
    GlobalSettings settings = defaults();
    if (!std::filesystem::exists(settings_path())) {
        if (error) {
            error->clear();
        }
        return settings;
    }

    std::string text;
    if (!dawn::infra::fs::read_text_file(settings_path(), &text, error)) {
        return settings;
    }

    const auto parsed = dawn::infra::json::parse(text);
    if (!parsed.ok) {
        if (error) {
            *error = parsed.error.message;
        }
        return settings;
    }

    if (!global_settings_from_json(parsed.value, &settings, error)) {
        return defaults();
    }
    return settings;
}

bool SettingsService::save(const GlobalSettings& settings, std::string* error) const {
    return dawn::infra::fs::write_text_file(settings_path(), dawn::infra::json::stringify(global_settings_to_json(settings), 2), error);
}

GlobalSettings SettingsService::defaults() const {
    GlobalSettings settings;
    settings.theme = "dark";
    settings.downloadConcurrency = 4;
    settings.cachePath = root_ / "cache";
    settings.javaStrategy = JavaStrategy::Auto;
    settings.memoryStrategy = MemoryStrategy::Balanced;
    settings.minMemoryMb = 2048;
    settings.maxMemoryMb = 4096;
    settings.backupStrategy = BackupStrategy::BeforeUpdate;
    settings.experimentalMode = false;
    return settings;
}

} // namespace dawn::core
