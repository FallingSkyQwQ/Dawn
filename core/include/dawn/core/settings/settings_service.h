#pragma once

#include <filesystem>
#include <string>
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
    ProxySettings proxy;
    bool experimentalMode = false;
    std::vector<std::string> experimentalFlags;
};

class SettingsService {
public:
    explicit SettingsService(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] std::filesystem::path settings_path() const;

    GlobalSettings load(std::string* error = nullptr) const;
    bool save(const GlobalSettings& settings, std::string* error = nullptr) const;
    GlobalSettings defaults() const;

private:
    std::filesystem::path root_;
};

} // namespace dawn::core
