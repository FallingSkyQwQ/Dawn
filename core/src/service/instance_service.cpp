#include "dawn/core/service/instance_service.h"

#include "dawn/core/service/preflight_service.h"
#include "dawn/infra/fs/file_system.h"

#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace dawn::core {

namespace {

std::string timestamp_now() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

std::string make_instance_id(const std::string& name) {
    std::string id;
    id.reserve(name.size() + 24);
    for (char ch : name) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            id.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!id.empty() && id.back() != '-') {
            id.push_back('-');
        }
    }
    if (id.empty()) {
        id = "instance";
    }
    id.push_back('-');
    id += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return id;
}

} // namespace

InstanceService::InstanceService(std::filesystem::path root) : repository_(std::move(root)) {}

const std::filesystem::path& InstanceService::root() const noexcept {
    return repository_.root();
}

std::vector<InstanceManifest> InstanceService::list_instances(std::string* error) const {
    return repository_.list_instances(error);
}

std::optional<InstanceManifest> InstanceService::load_instance(const std::string& id, std::string* error) const {
    return repository_.load_instance(id, error);
}

bool InstanceService::create_instance(InstanceManifest& manifest, std::string* error) const {
    if (manifest.id.empty()) {
        manifest.id = make_instance_id(manifest.name);
    }
    if (manifest.createdAt.empty()) {
        manifest.createdAt = timestamp_now();
    }
    if (manifest.gameDir.empty()) {
        manifest.gameDir = (repository_.root() / "instances" / manifest.id / "game").generic_string();
    }

    std::string local_error;
    const std::filesystem::path game_dir = manifest.gameDir;
    const std::filesystem::path directories[] = {
        game_dir,
        game_dir / "mods",
        game_dir / "resourcepacks",
        game_dir / "shaderpacks",
        game_dir / "saves",
        game_dir / "logs",
        game_dir / "config",
    };

    for (const auto& directory : directories) {
        if (!dawn::infra::fs::ensure_directory(directory, &local_error)) {
            if (error) {
                *error = local_error;
            }
            return false;
        }
    }

    return repository_.save_instance(manifest, error);
}

PreflightResult InstanceService::preflight(const InstanceManifest& manifest) const {
    return PreflightService{}.inspect(manifest);
}

LaunchCommand InstanceService::build_launch_command(const LaunchRequest& request) const {
    return runtime_.buildCommand(request);
}

} // namespace dawn::core
