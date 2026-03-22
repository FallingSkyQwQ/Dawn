#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dawn::core {

enum class JavaStrategy {
    Auto,
    Bundled,
    CustomPath,
    Downloaded,
};

enum class MemoryStrategy {
    Balanced,
    Conservative,
    Performance,
    Custom,
};

enum class BackupStrategy {
    Manual,
    BeforeLaunch,
    BeforeUpdate,
    Scheduled,
};

enum class UiMode {
    Novice,
    Advanced,
};

inline std::string_view to_string(UiMode mode) {
    switch (mode) {
    case UiMode::Novice: return "novice";
    case UiMode::Advanced: return "advanced";
    }
    return "novice";
}

inline UiMode ui_mode_from_string(std::string_view text) {
    return text == "advanced" ? UiMode::Advanced : UiMode::Novice;
}

struct ProxySettings {
    bool enabled = false;
    std::string scheme = "http";
    std::string host;
    int port = 0;
    std::string username;
    std::string password;
};

struct GlobalSettings {
    std::string theme = "dark";
    int downloadConcurrency = 4;
    std::filesystem::path cachePath;
    JavaStrategy javaStrategy = JavaStrategy::Auto;
    std::string javaRuntimePath;
    MemoryStrategy memoryStrategy = MemoryStrategy::Balanced;
    int minMemoryMb = 2048;
    int maxMemoryMb = 4096;
    BackupStrategy backupStrategy = BackupStrategy::BeforeUpdate;
    bool firstLaunchCompleted = false;
    UiMode uiMode = UiMode::Novice;
    int lowDiskThresholdGb = 20;
    ProxySettings proxy;
    bool experimentalMode = false;
    std::vector<std::string> experimentalFlags;
};

struct DiskSpaceCheckResult {
    bool low = false;
    std::filesystem::path path;
    std::uintmax_t availableBytes = 0;
    std::uintmax_t thresholdBytes = 0;
    std::string message;
};

struct CacheCleanupResult {
    bool success = false;
    std::filesystem::path cachePath;
    std::uintmax_t bytesBefore = 0;
    std::uintmax_t bytesAfter = 0;
    std::uintmax_t bytesFreed = 0;
    std::size_t filesRemoved = 0;
    std::size_t directoriesRemoved = 0;
    std::vector<std::string> logs;
    std::string message;
};

class SettingsService {
public:
    explicit SettingsService(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] std::filesystem::path settings_path() const;

    GlobalSettings load(std::string* error = nullptr) const;
    bool save(const GlobalSettings& settings, std::string* error = nullptr) const;
    GlobalSettings defaults() const;
    CacheCleanupResult clean_cache(const std::filesystem::path& cachePath, std::string* error = nullptr) const;
    static DiskSpaceCheckResult check_low_disk_space(const std::filesystem::path& path, int thresholdGb, std::string* error = nullptr);

private:
    std::filesystem::path root_;
};

} // namespace dawn::core
