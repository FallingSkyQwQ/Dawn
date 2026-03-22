#include "dawn/core/java/java_service.h"

#include <cstdlib>
#include <regex>
#include <set>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace dawn::core {

namespace {

std::filesystem::path java_binary_name() {
#if defined(_WIN32)
    return "java.exe";
#else
    return "java";
#endif
}

void push_if_exists(std::vector<std::filesystem::path>* paths, const std::filesystem::path& candidate) {
    if (!paths) {
        return;
    }
    std::error_code ec;
    if (std::filesystem::exists(candidate, ec) && !ec) {
        paths->push_back(candidate);
    }
}

std::vector<std::filesystem::path> discover_candidate_java_paths() {
    std::vector<std::filesystem::path> candidates;
    const auto javaName = java_binary_name();

    if (const char* javaHome = std::getenv("JAVA_HOME")) {
        push_if_exists(&candidates, std::filesystem::path(javaHome) / "bin" / javaName);
    }

    if (const char* pathValue = std::getenv("PATH")) {
        const char separator =
#if defined(_WIN32)
            ';';
#else
            ':';
#endif
        std::string pathText(pathValue);
        std::size_t start = 0;
        while (start <= pathText.size()) {
            const auto end = pathText.find(separator, start);
            const auto token = pathText.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if (!token.empty()) {
                push_if_exists(&candidates, std::filesystem::path(token) / javaName);
            }
            if (end == std::string::npos) {
                break;
            }
            start = end + 1;
        }
    }

#if defined(_WIN32)
    std::vector<const char*> windowsRoots = {
        std::getenv("ProgramFiles"),
        std::getenv("ProgramFiles(x86)"),
    };
    for (const auto* root : windowsRoots) {
        if (!root) {
            continue;
        }
        const std::filesystem::path rootPath(root);
        const std::vector<std::filesystem::path> vendors = {
            rootPath / "Java",
            rootPath / "Eclipse Adoptium",
            rootPath / "Microsoft",
            rootPath / "BellSoft",
        };
        for (const auto& vendor : vendors) {
            std::error_code ec;
            if (!std::filesystem::exists(vendor, ec) || ec) {
                continue;
            }
            for (const auto& entry : std::filesystem::directory_iterator(vendor, ec)) {
                if (ec || !entry.is_directory()) {
                    continue;
                }
                push_if_exists(&candidates, entry.path() / "bin" / javaName);
            }
        }
    }
#else
    const std::vector<std::filesystem::path> commonUnixPaths = {
        "/usr/bin/java",
        "/usr/local/bin/java",
        "/opt/homebrew/bin/java",
        "/Library/Java/JavaVirtualMachines",
    };
    for (const auto& path : commonUnixPaths) {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
                if (ec || !entry.is_directory()) {
                    continue;
                }
                push_if_exists(&candidates, entry.path() / "Contents" / "Home" / "bin" / javaName);
            }
            continue;
        }
        push_if_exists(&candidates, path);
    }
#endif

    return candidates;
}

int parse_major_from_path(const std::filesystem::path& executable) {
    const auto text = executable.generic_string();
    std::regex pattern(R"((?:jdk|jre|temurin|java)[-_]?(\d{1,2}))", std::regex::icase);
    std::smatch match;
    if (std::regex_search(text, match, pattern) && match.size() >= 2) {
        return std::stoi(match[1].str());
    }
    return 0;
}

std::string infer_vendor(const std::filesystem::path& executable) {
    const auto text = executable.generic_string();
    if (text.find("Adoptium") != std::string::npos || text.find("temurin") != std::string::npos) {
        return "adoptium";
    }
    if (text.find("Microsoft") != std::string::npos) {
        return "microsoft";
    }
    if (text.find("BellSoft") != std::string::npos) {
        return "bellsoft";
    }
    if (text.find("Java") != std::string::npos || text.find("jdk") != std::string::npos) {
        return "system";
    }
    return "unknown";
}

} // namespace

std::vector<JavaRuntimeInfo> JavaService::discover_runtimes() const {
    std::vector<JavaRuntimeInfo> runtimes;
    std::set<std::string> seen;
    int index = 0;

    for (const auto& candidate : discover_candidate_java_paths()) {
        std::error_code ec;
        const auto canonicalPath = std::filesystem::weakly_canonical(candidate, ec);
        const auto canonical = ec ? candidate.generic_string() : canonicalPath.generic_string();
        if (canonical.empty() || !seen.insert(canonical).second) {
            continue;
        }

        JavaRuntimeInfo runtime;
        runtime.id = "java-runtime-" + std::to_string(++index);
        runtime.executable = candidate;
        runtime.vendor = infer_vendor(candidate);
        runtime.majorVersion = parse_major_from_path(candidate);
        runtime.versionText = runtime.majorVersion > 0 ? std::to_string(runtime.majorVersion) : "unknown";
        runtime.bundled = false;
        runtime.available = true;
        runtimes.push_back(std::move(runtime));
    }

    return runtimes;
}

JavaRuntimeInfo JavaService::recommended_runtime() const {
    const auto runtimes = discover_runtimes();
    if (!runtimes.empty()) {
        return runtimes.front();
    }
    return JavaRuntimeInfo{
        .id = "fallback-java",
        .executable = std::filesystem::path("java"),
        .vendor = "fallback",
        .versionText = "unknown",
        .majorVersion = 17,
        .bundled = false,
        .available = false,
    };
}

JavaProfile JavaService::build_profile(const JavaRuntimeInfo& runtime) const {
    JavaProfile profile;
    profile.profileId = runtime.id.empty() ? "java-profile" : runtime.id + "-profile";
    profile.runtimeId = runtime.id;
    profile.minimumMajor = runtime.majorVersion > 0 ? runtime.majorVersion : 17;
    profile.maximumMajor = runtime.majorVersion > 0 ? runtime.majorVersion : 21;
    profile.autoDiscover = !runtime.bundled;
    profile.notes = runtime.vendor.empty() ? "managed by dawn" : runtime.vendor;
    return profile;
}

bool JavaService::is_compatible(const JavaProfile& profile, const JavaRuntimeInfo& runtime) const {
    if (!runtime.available) {
        return false;
    }
    if (runtime.majorVersion == 0) {
        return true;
    }
    return runtime.majorVersion >= profile.minimumMajor && runtime.majorVersion <= profile.maximumMajor;
}

} // namespace dawn::core
