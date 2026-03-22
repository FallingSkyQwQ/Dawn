#pragma once

#include "dawn/core/model/instance_manifest.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace dawn::core {

class InstanceRepository {
public:
    explicit InstanceRepository(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] std::filesystem::path instances_directory() const;

    std::vector<InstanceManifest> list_instances(std::string* error = nullptr) const;
    std::optional<InstanceManifest> load_instance(const std::string& id, std::string* error = nullptr) const;
    bool save_instance(const InstanceManifest& manifest, std::string* error = nullptr) const;
    bool remove_instance(const std::string& id, std::string* error = nullptr) const;

private:
    std::filesystem::path root_;
};

} // namespace dawn::core
