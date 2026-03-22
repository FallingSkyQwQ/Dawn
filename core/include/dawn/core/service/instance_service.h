#pragma once

#include "dawn/core/model/preflight.h"
#include "dawn/core/storage/instance_repository.h"
#include "dawn/core/interfaces/default_launcher_runtime.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dawn::core {

class InstanceService {
public:
    explicit InstanceService(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    std::vector<InstanceManifest> list_instances(std::string* error = nullptr) const;
    std::optional<InstanceManifest> load_instance(const std::string& id, std::string* error = nullptr) const;
    bool create_instance(InstanceManifest& manifest, std::string* error = nullptr) const;
    PreflightResult preflight(const InstanceManifest& manifest) const;
    LaunchCommand build_launch_command(const LaunchRequest& request) const;

private:
    InstanceRepository repository_;
    DefaultLauncherRuntime runtime_;
};

} // namespace dawn::core
