#include "dawn/core/local/local_package_service.h"

#include <algorithm>
#include <cctype>
#include <initializer_list>
#include <fstream>
#include <iterator>

namespace dawn::core {

namespace {

std::string to_lower_copy(std::string text) {
    for (char& ch : text) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return text;
}

bool contains(const std::string& haystack, std::string_view needle) {
    return haystack.find(needle) != std::string::npos;
}

bool contains_any(const std::string& haystack, std::initializer_list<std::string_view> needles) {
    for (const auto needle : needles) {
        if (contains(haystack, needle)) {
            return true;
        }
    }
    return false;
}

void add_reason(LocalPackageAnalysis* analysis, std::string reason, double weight) {
    if (!analysis) {
        return;
    }
    analysis->reasons.push_back(std::move(reason));
    analysis->confidence = std::min(1.0, analysis->confidence + weight);
}

std::string read_file_bytes(const std::filesystem::path& path, std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (error) {
            *error = "failed to open file: " + path.string();
        }
        return {};
    }

    std::string contents;
    stream.seekg(0, std::ios::end);
    const auto size = stream.tellg();
    if (size < 0) {
        if (error) {
            *error = "failed to determine file size: " + path.string();
        }
        return {};
    }
    contents.resize(static_cast<std::size_t>(size));
    stream.seekg(0, std::ios::beg);
    stream.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!stream.good() && !stream.eof()) {
        if (error) {
            *error = "failed to read file: " + path.string();
        }
        return {};
    }
    if (error) {
        error->clear();
    }
    return contents;
}

LocalPackageType infer_from_extensions(const std::filesystem::path& path, const std::string& lowerName) {
    const auto ext = to_lower_copy(path.extension().string());
    if (ext == ".mrpack") {
        return LocalPackageType::Modpack;
    }
    if (ext != ".zip" && ext != ".jar") {
        return LocalPackageType::Unknown;
    }
    if (contains(lowerName, "modpack") || (contains(lowerName, "pack") && contains(lowerName, "mod"))) {
        return LocalPackageType::Modpack;
    }
    if (contains(lowerName, "shader")) {
        return LocalPackageType::Shader;
    }
    if (contains(lowerName, "resource") || contains(lowerName, "texture")) {
        return LocalPackageType::Resourcepack;
    }
    if (ext == ".jar" && (contains(lowerName, "mod") || contains(lowerName, "fabric") || contains(lowerName, "forge") || contains(lowerName, "quilt") || contains(lowerName, "neoforge"))) {
        return LocalPackageType::Mod;
    }
    return LocalPackageType::Unknown;
}

} // namespace

LocalPackageAnalysis LocalPackageService::analyze(const std::filesystem::path& path) const {
    LocalPackageAnalysis analysis;
    analysis.path = path;
    analysis.displayName = path.stem().string();

    if (path.empty()) {
        add_reason(&analysis, "path is empty", 0.0);
        return analysis;
    }

    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) {
        add_reason(&analysis, "file does not exist", 0.0);
        return analysis;
    }
    if (!std::filesystem::is_regular_file(path, ec)) {
        add_reason(&analysis, "path is not a regular file", 0.0);
        return analysis;
    }

    const auto lowerName = to_lower_copy(path.filename().string());
    const auto ext = to_lower_copy(path.extension().string());
    analysis.archive = ext == ".zip" || ext == ".jar" || ext == ".mrpack";
    analysis.type = infer_from_extensions(path, lowerName);

    std::string error;
    const auto contents = read_file_bytes(path, &error);
    if (!error.empty()) {
        add_reason(&analysis, error, 0.0);
        return analysis;
    }

    const bool hasModpackManifest = contains_any(contents, {"modrinth.index.json", "manifest.json", "mmc-pack.json"});
    const bool hasModManifest = contains_any(contents, {"fabric.mod.json", "quilt.mod.json", "mods.toml", "neoforge.mods.toml"});
    const bool hasResourcepackManifest = contains_any(contents, {"pack.mcmeta", "textures/", "assets/"});
    const bool hasShaderManifest = contains_any(contents, {"shaders.properties", "shaderpacks/", "shaders/"});

    if (hasModpackManifest) {
        analysis.type = LocalPackageType::Modpack;
        add_reason(&analysis, "modpack manifest detected inside archive", 0.6);
    } else if (hasModManifest) {
        analysis.type = LocalPackageType::Mod;
        add_reason(&analysis, "mod loader metadata detected inside archive", 0.5);
    } else if (hasShaderManifest) {
        analysis.type = LocalPackageType::Shader;
        add_reason(&analysis, "shader pack metadata detected inside archive", 0.4);
    } else if (hasResourcepackManifest) {
        analysis.type = LocalPackageType::Resourcepack;
        add_reason(&analysis, "resource pack metadata detected inside archive", 0.4);
    }

    if (analysis.type == LocalPackageType::Unknown) {
        analysis.type = infer_from_extensions(path, lowerName);
        if (analysis.type != LocalPackageType::Unknown) {
            add_reason(&analysis, "heuristics inferred type from filename or extension", 0.2);
        }
    }

    if (analysis.type == LocalPackageType::Unknown) {
        add_reason(&analysis, "could not confidently classify the local package", 0.0);
    } else {
        add_reason(&analysis, std::string("classified as ") + std::string(to_string(analysis.type)), 0.1);
    }

    if (!contains_any(contents, {"modrinth.index.json", "manifest.json", "mmc-pack.json",
                                 "fabric.mod.json", "quilt.mod.json", "mods.toml", "neoforge.mods.toml",
                                 "pack.mcmeta", "shaders.properties", "shaderpacks/", "shaders/"})) {
        add_reason(&analysis, "no well-known archive signature was detected", 0.0);
    }

    return analysis;
}

ProjectType LocalPackageService::project_type_for(LocalPackageType type) {
    switch (type) {
    case LocalPackageType::Mod: return ProjectType::Mod;
    case LocalPackageType::Resourcepack: return ProjectType::Resourcepack;
    case LocalPackageType::Shader: return ProjectType::Shader;
    case LocalPackageType::Modpack: return ProjectType::Modpack;
    case LocalPackageType::Unknown: return ProjectType::Mod;
    }
    return ProjectType::Mod;
}

std::string_view LocalPackageService::describe(LocalPackageType type) {
    return to_string(type);
}

} // namespace dawn::core
