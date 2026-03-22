#include "dawn/core/settings/settings_service.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <algorithm>
#include <limits>
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

std::string to_string(UiMode mode) {
    switch (mode) {
    case UiMode::Novice: return "novice";
    case UiMode::Advanced: return "advanced";
    }
    return "novice";
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

UiMode ui_mode_from_string(const std::string& text) {
    if (text == "advanced") return UiMode::Advanced;
    return UiMode::Novice;
}

std::uintmax_t clamp_threshold_bytes(int thresholdGb) {
    if (thresholdGb <= 0) {
        return 0;
    }

    constexpr std::uintmax_t kGb = 1024ull * 1024ull * 1024ull;
    const auto maxValue = std::numeric_limits<std::uintmax_t>::max();
    const auto threshold = static_cast<std::uintmax_t>(thresholdGb);
    if (threshold > maxValue / kGb) {
        return maxValue;
    }
    return threshold * kGb;
}

std::uintmax_t path_size(const std::filesystem::path& path, std::string* error = nullptr) {
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        if (error) {
            error->clear();
        }
        return 0;
    }

    if (std::filesystem::is_regular_file(path, ec)) {
        return dawn::infra::fs::file_size(path, error);
    }

    std::uintmax_t total = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
        if (ec) {
            if (error) {
                *error = ec.message();
            }
            return total;
        }
        if (entry.is_regular_file()) {
            total += dawn::infra::fs::file_size(entry.path(), nullptr);
        }
    }
    if (error) {
        error->clear();
    }
    return total;
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
    object.emplace("backupScheduleDate", Value(settings.backupScheduleDate));
    object.emplace("backupScheduleTime", Value(settings.backupScheduleTime));
    object.emplace("firstLaunchCompleted", Value(settings.firstLaunchCompleted));
    object.emplace("uiMode", Value(std::string(dawn::core::to_string(settings.uiMode))));
    object.emplace("lowDiskThresholdGb", Value(settings.lowDiskThresholdGb));
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
    const auto* backup_schedule_date = dawn::infra::json::find(object, "backupScheduleDate");
    const auto* backup_schedule_time = dawn::infra::json::find(object, "backupScheduleTime");
    const auto* first_launch_completed = dawn::infra::json::find(object, "firstLaunchCompleted");
    const auto* ui_mode = dawn::infra::json::find(object, "uiMode");
    const auto* low_disk_threshold_gb = dawn::infra::json::find(object, "lowDiskThresholdGb");
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
    settings->backupScheduleDate = backup_schedule_date && backup_schedule_date->is_string() ? backup_schedule_date->as_string() : std::string{};
    settings->backupScheduleTime = backup_schedule_time && backup_schedule_time->is_string() ? backup_schedule_time->as_string() : std::string("03:00");
    settings->firstLaunchCompleted = first_launch_completed && first_launch_completed->is_bool() ? first_launch_completed->as_bool() : false;
    settings->uiMode = ui_mode && ui_mode->is_string() ? ui_mode_from_string(ui_mode->as_string()) : UiMode::Novice;
    settings->lowDiskThresholdGb = low_disk_threshold_gb && low_disk_threshold_gb->is_number() ? static_cast<int>(low_disk_threshold_gb->as_number()) : 20;

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
    settings.backupScheduleDate.clear();
    settings.backupScheduleTime = "03:00";
    settings.firstLaunchCompleted = false;
    settings.uiMode = UiMode::Novice;
    settings.lowDiskThresholdGb = 20;
    settings.experimentalMode = false;
    return settings;
}

CacheCleanupResult SettingsService::clean_cache(const std::filesystem::path& cachePath, std::string* error) const {
    CacheCleanupResult result;
    result.cachePath = cachePath;
    result.bytesBefore = path_size(cachePath, error);

    if (cachePath.empty()) {
        result.success = true;
        result.message = "cache path is empty";
        result.bytesAfter = result.bytesBefore;
        if (error) {
            error->clear();
        }
        return result;
    }

    std::error_code ec;
    if (!std::filesystem::exists(cachePath, ec)) {
        result.success = true;
        result.message = "cache directory does not exist";
        result.bytesAfter = result.bytesBefore;
        if (error) {
            error->clear();
        }
        return result;
    }

    if (!std::filesystem::is_directory(cachePath, ec)) {
        const auto bytesFreed = dawn::infra::fs::file_size(cachePath, &result.message);
        if (!result.message.empty()) {
            if (error) {
                *error = result.message;
            }
            return result;
        }
        if (!std::filesystem::remove(cachePath, ec)) {
            result.message = ec ? ec.message() : "failed to remove cache file";
            if (error) {
                *error = result.message;
            }
            return result;
        }
        result.success = true;
        result.filesRemoved = 1;
        result.bytesFreed = bytesFreed;
        result.bytesAfter = 0;
        result.message = "removed cache file";
        if (error) {
            error->clear();
        }
        return result;
    }

    std::vector<std::filesystem::path> children;
    for (const auto& entry : std::filesystem::directory_iterator(cachePath, ec)) {
        if (ec) {
            break;
        }
        children.push_back(entry.path());
    }
    if (ec) {
        result.message = ec.message();
        if (error) {
            *error = result.message;
        }
        return result;
    }

    for (const auto& child : children) {
        std::uintmax_t childBytes = 0;
        std::size_t childFiles = 0;
        std::size_t childDirs = 0;
        bool childIsDirectory = false;

        std::error_code walkEc;
        if (std::filesystem::is_regular_file(child, walkEc)) {
            childBytes = dawn::infra::fs::file_size(child, &result.message);
            if (!result.message.empty()) {
                result.logs.push_back("scan failed for " + child.generic_string() + ": " + result.message);
                result.message.clear();
            }
            ++childFiles;
        } else if (std::filesystem::is_directory(child, walkEc)) {
            childIsDirectory = true;
            for (const auto& descendant : std::filesystem::recursive_directory_iterator(child, walkEc)) {
                if (walkEc) {
                    break;
                }
                if (descendant.is_regular_file()) {
                    childBytes += dawn::infra::fs::file_size(descendant.path(), nullptr);
                    ++childFiles;
                } else if (descendant.is_directory()) {
                    ++childDirs;
                }
            }
        }

        const auto removed = std::filesystem::remove_all(child, ec);
        if (ec) {
            result.message = ec.message();
            if (error) {
                *error = result.message;
            }
            return result;
        }

        result.bytesFreed += childBytes;
        result.filesRemoved += childFiles;
        result.directoriesRemoved += childDirs + (childIsDirectory && removed > 0 ? 1 : 0);
        result.logs.push_back("removed " + child.generic_string());
    }

    result.success = true;
    result.bytesAfter = 0;
    result.bytesFreed = result.bytesBefore;
    result.message = "cache cleaned";
    if (error) {
        error->clear();
    }
    return result;
}

DiskSpaceCheckResult SettingsService::check_low_disk_space(const std::filesystem::path& path, int thresholdGb, std::string* error) {
    DiskSpaceCheckResult result;
    result.path = path;
    result.thresholdBytes = clamp_threshold_bytes(thresholdGb);

    std::filesystem::path probe = path;
    std::error_code ec;
    if (probe.empty() || !std::filesystem::exists(probe, ec)) {
        probe = probe.parent_path();
        if (probe.empty()) {
            probe = std::filesystem::current_path();
        }
    }

    const auto space = std::filesystem::space(probe, ec);
    if (ec) {
        result.message = ec.message();
        if (error) {
            *error = result.message;
        }
        return result;
    }

    result.availableBytes = space.available;
    result.low = result.thresholdBytes > 0 && result.availableBytes < result.thresholdBytes;
    const auto availableGb = static_cast<double>(result.availableBytes) / (1024.0 * 1024.0 * 1024.0);
    const auto thresholdGbValue = static_cast<double>(thresholdGb < 0 ? 0 : thresholdGb);
    result.message = result.low
        ? "low disk space: available " + std::to_string(static_cast<int>(availableGb)) + " GB below threshold " + std::to_string(static_cast<int>(thresholdGbValue)) + " GB"
        : "disk space is sufficient";
    if (error) {
        error->clear();
    }
    return result;
}

} // namespace dawn::core
