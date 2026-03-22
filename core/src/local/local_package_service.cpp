#include "dawn/core/local/local_package_service.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <initializer_list>
#include <iterator>

namespace dawn::core {

namespace {

constexpr std::uint32_t kEndOfCentralDirectorySignature = 0x06054b50;
constexpr std::uint32_t kCentralDirectoryFileHeaderSignature = 0x02014b50;

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

std::uint16_t read_u16(const std::string& data, std::size_t offset) {
    return static_cast<std::uint16_t>(
        static_cast<unsigned char>(data[offset]) |
        (static_cast<std::uint16_t>(static_cast<unsigned char>(data[offset + 1])) << 8));
}

std::uint32_t read_u32(const std::string& data, std::size_t offset) {
    return static_cast<std::uint32_t>(
        static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset])) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 1])) << 8) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 2])) << 16) |
        (static_cast<std::uint32_t>(static_cast<unsigned char>(data[offset + 3])) << 24));
}

bool parse_central_directory_entries(const std::string& bytes, std::vector<std::string>* entries) {
    if (!entries || bytes.size() < 22) {
        return false;
    }

    const std::size_t searchStart = bytes.size() > 0xFFFF + 22 ? bytes.size() - (0xFFFF + 22) : 0;
    std::size_t eocdOffset = std::string::npos;
    for (std::size_t index = bytes.size() - 22; index + 4 <= bytes.size(); --index) {
        if (read_u32(bytes, index) == kEndOfCentralDirectorySignature) {
            eocdOffset = index;
            break;
        }
        if (index == searchStart) {
            break;
        }
    }
    if (eocdOffset == std::string::npos || eocdOffset + 22 > bytes.size()) {
        return false;
    }

    const auto centralDirectorySize = static_cast<std::size_t>(read_u32(bytes, eocdOffset + 12));
    const auto centralDirectoryOffset = static_cast<std::size_t>(read_u32(bytes, eocdOffset + 16));
    if (centralDirectoryOffset >= bytes.size() || centralDirectoryOffset + centralDirectorySize > bytes.size()) {
        return false;
    }

    std::size_t offset = centralDirectoryOffset;
    const auto limit = centralDirectoryOffset + centralDirectorySize;
    while (offset + 46 <= limit) {
        if (read_u32(bytes, offset) != kCentralDirectoryFileHeaderSignature) {
            return false;
        }

        const auto nameLength = static_cast<std::size_t>(read_u16(bytes, offset + 28));
        const auto extraLength = static_cast<std::size_t>(read_u16(bytes, offset + 30));
        const auto commentLength = static_cast<std::size_t>(read_u16(bytes, offset + 32));
        const auto entryStart = offset + 46;
        const auto entryEnd = entryStart + nameLength;
        if (entryEnd > limit) {
            return false;
        }

        entries->push_back(bytes.substr(entryStart, nameLength));
        offset = entryEnd + extraLength + commentLength;
    }

    return !entries->empty();
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

std::string join_entries(const std::vector<std::string>& entries) {
    std::string result;
    for (const auto& entry : entries) {
        if (!result.empty()) {
            result += ", ";
        }
        result += entry;
    }
    return result;
}

void classify_from_entries(LocalPackageAnalysis* analysis, const std::vector<std::string>& entries) {
    if (!analysis || entries.empty()) {
        return;
    }

    std::vector<std::string> lowerEntries;
    lowerEntries.reserve(entries.size());
    for (const auto& entry : entries) {
        lowerEntries.push_back(to_lower_copy(entry));
    }

    auto has_entry = [&](std::string_view needle) {
        return std::any_of(lowerEntries.begin(), lowerEntries.end(), [&](const std::string& entry) {
            return entry == needle || contains(entry, needle);
        });
    };

    auto has_prefix = [&](std::string_view needle) {
        return std::any_of(lowerEntries.begin(), lowerEntries.end(), [&](const std::string& entry) {
            return entry.rfind(needle, 0) == 0 || contains(entry, needle);
        });
    };

    if (has_entry("modrinth.index.json") || has_entry("manifest.json") || has_entry("mmc-pack.json")) {
        analysis->type = LocalPackageType::Modpack;
        add_reason(analysis, "central directory contains a modpack manifest", 0.75);
        return;
    }

    if (has_entry("fabric.mod.json") || has_entry("quilt.mod.json") || has_entry("mods.toml") || has_entry("neoforge.mods.toml")) {
        analysis->type = LocalPackageType::Mod;
        add_reason(analysis, "central directory contains loader metadata", 0.70);
        return;
    }

    if (has_entry("pack.mcmeta")) {
        analysis->type = LocalPackageType::Resourcepack;
        add_reason(analysis, "central directory contains pack.mcmeta", 0.75);
        return;
    }

    if (has_prefix("assets/") || has_prefix("textures/") || has_prefix("overlay/")) {
        analysis->type = LocalPackageType::Resourcepack;
        add_reason(analysis, "archive paths match a resource pack layout", 0.55);
        return;
    }

    if (has_entry("shaders.properties") || has_prefix("shaders/") || has_prefix("shaderpacks/")) {
        analysis->type = LocalPackageType::Shader;
        add_reason(analysis, "archive paths match a shader pack layout", 0.60);
        return;
    }
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

    if (analysis.archive) {
        if (parse_central_directory_entries(contents, &analysis.archiveEntries)) {
            add_reason(&analysis, "central directory entries: " + join_entries(analysis.archiveEntries), 0.05);
            classify_from_entries(&analysis, analysis.archiveEntries);
        } else {
            add_reason(&analysis, "failed to parse central directory; falling back to filename heuristics", 0.0);
        }
    }

    if (analysis.type == LocalPackageType::Unknown) {
        analysis.type = infer_from_extensions(path, lowerName);
        if (analysis.type != LocalPackageType::Unknown) {
            add_reason(&analysis, "heuristics inferred type from filename or extension", 0.20);
        }
    }

    if (analysis.type == LocalPackageType::Unknown) {
        add_reason(&analysis, "could not confidently classify the local package", 0.0);
    } else {
        add_reason(&analysis, std::string("classified as ") + std::string(to_string(analysis.type)), 0.10);
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
