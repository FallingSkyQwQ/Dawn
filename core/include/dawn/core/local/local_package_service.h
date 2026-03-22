#pragma once

#include "dawn/core/model/enums.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace dawn::core {

enum class LocalPackageType {
    Unknown,
    Mod,
    Resourcepack,
    Shader,
    Modpack,
};

inline std::string_view to_string(LocalPackageType type) {
    switch (type) {
    case LocalPackageType::Unknown: return "unknown";
    case LocalPackageType::Mod: return "mod";
    case LocalPackageType::Resourcepack: return "resourcepack";
    case LocalPackageType::Shader: return "shader";
    case LocalPackageType::Modpack: return "modpack";
    }
    return "unknown";
}

inline LocalPackageType local_package_type_from_string(std::string_view text) {
    if (text == "mod") return LocalPackageType::Mod;
    if (text == "resourcepack") return LocalPackageType::Resourcepack;
    if (text == "shader") return LocalPackageType::Shader;
    if (text == "modpack") return LocalPackageType::Modpack;
    return LocalPackageType::Unknown;
}

struct LocalPackageAnalysis {
    std::filesystem::path path;
    LocalPackageType type = LocalPackageType::Unknown;
    std::string displayName;
    bool archive = false;
    double confidence = 0.0;
    std::vector<std::string> reasons;
    std::vector<std::string> archiveEntries;
};

class LocalPackageService {
public:
    [[nodiscard]] LocalPackageAnalysis analyze(const std::filesystem::path& path) const;

    [[nodiscard]] static ProjectType project_type_for(LocalPackageType type);
    [[nodiscard]] static std::string_view describe(LocalPackageType type);
};

} // namespace dawn::core
