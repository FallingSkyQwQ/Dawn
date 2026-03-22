#include "dawn/core/storage/instance_repository.h"

#include "dawn/core/serialization/manifest_codec.h"
#include "dawn/infra/fs/file_system.h"

#include <algorithm>
#include <utility>

namespace dawn::core {

InstanceRepository::InstanceRepository(std::filesystem::path root) : root_(std::move(root)) {}

const std::filesystem::path& InstanceRepository::root() const noexcept {
    return root_;
}

std::filesystem::path InstanceRepository::instances_directory() const {
    return root_ / "instances";
}

std::vector<InstanceManifest> InstanceRepository::list_instances(std::string* error) const {
    std::vector<InstanceManifest> result;
    for (const auto& path : dawn::infra::fs::list_files(instances_directory(), ".json")) {
        InstanceManifest manifest;
        if (!load_instance_manifest(path, &manifest, error)) {
            continue;
        }
        result.push_back(std::move(manifest));
    }
    std::sort(result.begin(), result.end(), [](const InstanceManifest& lhs, const InstanceManifest& rhs) {
        return lhs.createdAt < rhs.createdAt;
    });
    return result;
}

std::optional<InstanceManifest> InstanceRepository::load_instance(const std::string& id, std::string* error) const {
    const auto path = instances_directory() / (id + ".json");
    InstanceManifest manifest;
    if (!load_instance_manifest(path, &manifest, error)) {
        return std::nullopt;
    }
    return manifest;
}

bool InstanceRepository::save_instance(const InstanceManifest& manifest, std::string* error) const {
    std::string local_error;
    if (!dawn::infra::fs::ensure_directory(instances_directory(), &local_error)) {
        if (error) {
            *error = local_error;
        }
        return false;
    }
    return save_instance_manifest(instances_directory() / (manifest.id + ".json"), manifest, error);
}

bool InstanceRepository::remove_instance(const std::string& id, std::string* error) const {
    std::error_code ec;
    const auto path = instances_directory() / (id + ".json");
    const bool removed = std::filesystem::remove(path, ec);
    if (!removed && ec) {
        if (error) {
            *error = ec.message();
        }
        return false;
    }
    return true;
}

} // namespace dawn::core
