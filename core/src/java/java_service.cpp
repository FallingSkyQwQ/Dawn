#include "dawn/core/java/java_service.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"
#include "dawn/infra/net/http_client.h"
#include "dawn/infra/net/http_client_factory.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <unistd.h>
#include <sys/utsname.h>
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

// Parse Java version from java -version output
// Example outputs:
//   openjdk version "17.0.8" 2023-07-18
//   java version "1.8.0_381"
//   openjdk version "21.0.1" 2023-10-17 LTS
int parse_java_version_output(const std::string& versionOutput, std::string& fullVersion, std::string& vendor) {
    // Try to find version string pattern
    std::regex versionPattern(R"((?:version|openjdk version)\s+"(\d+)(?:\.(\d+))?(?:\.(\d+))?(?:[_\.](\d+))?[^"]*")");
    std::smatch match;

    if (std::regex_search(versionOutput, match, versionPattern)) {
        fullVersion = match[0];
        int major = 0;

        // Check if it's old Java 1.x format
        if (match[1] == "1" && match[2].matched) {
            major = std::stoi(match[2].str());
        } else {
            major = std::stoi(match[1].str());
        }

        // Detect vendor from output
        if (versionOutput.find("Adoptium") != std::string::npos ||
            versionOutput.find("Temurin") != std::string::npos) {
            vendor = "adoptium";
        } else if (versionOutput.find("Microsoft") != std::string::npos) {
            vendor = "microsoft";
        } else if (versionOutput.find("BellSoft") != std::string::npos ||
                   versionOutput.find("Liberica") != std::string::npos) {
            vendor = "bellsoft";
        } else if (versionOutput.find("Oracle") != std::string::npos) {
            vendor = "oracle";
        } else if (versionOutput.find("Amazon") != std::string::npos ||
                   versionOutput.find("Corretto") != std::string::npos) {
            vendor = "amazon";
        } else if (versionOutput.find("Azul") != std::string::npos ||
                   versionOutput.find("Zulu") != std::string::npos) {
            vendor = "azul";
        } else if (versionOutput.find("openjdk") != std::string::npos) {
            vendor = "openjdk";
        } else {
            vendor = "unknown";
        }

        return major;
    }

    return 0;
}

// Execute java -version and capture output
std::pair<int, std::string> execute_java_version(const std::filesystem::path& executable) {
    std::string command = executable.string() + " -version 2>&1";

#if defined(_WIN32)
    // Windows implementation using _popen
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        return {-1, ""};
    }

    std::array<char, 256> buffer;
    std::string result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        result += buffer.data();
    }

#if defined(_WIN32)
    int exitCode = _pclose(pipe);
#else
    int exitCode = pclose(pipe);
#endif

    // Normalize exit code
    if (exitCode == -1) {
        return {exitCode, result};
    }
#if !defined(_WIN32)
    exitCode = WEXITSTATUS(exitCode);
#endif

    return {exitCode, result};
}

std::vector<std::filesystem::path> discover_candidate_java_paths() {
    std::vector<std::filesystem::path> candidates;
    const auto javaName = java_binary_name();

    // Check JAVA_HOME
    if (const char* javaHome = std::getenv("JAVA_HOME")) {
        push_if_exists(&candidates, std::filesystem::path(javaHome) / "bin" / javaName);
    }

    // Check PATH
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
    // Windows registry check for Java installations
    HKEY hKey;
    const char* registryPaths[] = {
        "SOFTWARE\\JavaSoft\\Java Runtime Environment",
        "SOFTWARE\\JavaSoft\\Java Development Kit",
        "SOFTWARE\\JavaSoft\\JDK",
        "SOFTWARE\\Eclipse Adoptium\\JDK",
        "SOFTWARE\\Microsoft\\JDK",
        "SOFTWARE\\BellSoft\\Liberica"
    };

    for (const auto* regPath : registryPaths) {
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
            char keyName[256];
            DWORD keyNameSize = sizeof(keyName);
            DWORD index = 0;

            while (RegEnumKeyA(hKey, index, keyName, keyNameSize) == ERROR_SUCCESS) {
                HKEY versionKey;
                if (RegOpenKeyExA(hKey, keyName, 0, KEY_READ, &versionKey) == ERROR_SUCCESS) {
                    char javaHomePath[512];
                    DWORD valueSize = sizeof(javaHomePath);
                    DWORD valueType;

                    if (RegQueryValueExA(versionKey, "JavaHome", nullptr, &valueType,
                                        reinterpret_cast<LPBYTE>(javaHomePath), &valueSize) == ERROR_SUCCESS) {
                        push_if_exists(&candidates, std::filesystem::path(javaHomePath) / "bin" / javaName);
                    }
                    RegCloseKey(versionKey);
                }
                index++;
            }
            RegCloseKey(hKey);
        }

        // Also check 32-bit registry on 64-bit Windows
        if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, regPath, 0, KEY_READ | KEY_WOW64_32KEY, &hKey) == ERROR_SUCCESS) {
            char keyName[256];
            DWORD keyNameSize = sizeof(keyName);
            DWORD index = 0;

            while (RegEnumKeyA(hKey, index, keyName, keyNameSize) == ERROR_SUCCESS) {
                HKEY versionKey;
                if (RegOpenKeyExA(hKey, keyName, 0, KEY_READ, &versionKey) == ERROR_SUCCESS) {
                    char javaHomePath[512];
                    DWORD valueSize = sizeof(javaHomePath);
                    DWORD valueType;

                    if (RegQueryValueExA(versionKey, "JavaHome", nullptr, &valueType,
                                        reinterpret_cast<LPBYTE>(javaHomePath), &valueSize) == ERROR_SUCCESS) {
                        push_if_exists(&candidates, std::filesystem::path(javaHomePath) / "bin" / javaName);
                    }
                    RegCloseKey(versionKey);
                }
                index++;
            }
            RegCloseKey(hKey);
        }
    }

    // Common Windows installation directories
    std::vector<const char*> windowsRoots = {
        std::getenv("ProgramFiles"),
        std::getenv("ProgramFiles(x86)"),
        std::getenv("LOCALAPPDATA"),
    };

    for (const auto* root : windowsRoots) {
        if (!root) {
            continue;
        }
        const std::filesystem::path rootPath(root);
        const std::vector<std::filesystem::path> vendors = {
            rootPath / "Java",
            rootPath / "Eclipse Adoptium",
            rootPath / "Eclipse Foundation",
            rootPath / "Microsoft",
            rootPath / "BellSoft",
            rootPath / "Amazon Corretto",
            rootPath / "Zulu",
            rootPath / "Oracle",
            rootPath / "JavaSoft",
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
                // Check for nested structure (e.g., jdk-17/bin/java.exe)
                push_if_exists(&candidates, entry.path() / "bin" / javaName);

                // Also check direct subdirectories
                for (const auto& subEntry : std::filesystem::directory_iterator(entry.path(), ec)) {
                    if (ec || !subEntry.is_directory()) {
                        continue;
                    }
                    push_if_exists(&candidates, subEntry.path() / "bin" / javaName);
                }
            }
        }
    }
#else
    // Unix/Linux/macOS common paths
    const std::vector<std::filesystem::path> commonUnixPaths = {
        "/usr/bin/java",
        "/usr/local/bin/java",
        "/opt/homebrew/bin/java",
        "/usr/lib/jvm",
        "/usr/local/lib/jvm",
        "/opt",
        "/Library/Java/JavaVirtualMachines",
        "/System/Library/Java/JavaVirtualMachines",
    };

    for (const auto& path : commonUnixPaths) {
        std::error_code ec;
        if (std::filesystem::is_directory(path, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(path, ec)) {
                if (ec || !entry.is_directory()) {
                    continue;
                }
                // Linux structure: /usr/lib/jvm/java-17-openjdk/bin/java
                push_if_exists(&candidates, entry.path() / "bin" / javaName);
                // macOS structure: /Library/Java/JavaVirtualMachines/jdk-17.jdk/Contents/Home/bin/java
                push_if_exists(&candidates, entry.path() / "Contents" / "Home" / "bin" / javaName);
            }
            continue;
        }
        push_if_exists(&candidates, path);
    }

    // Check SDKMAN installations
    if (const char* sdkmanDir = std::getenv("SDKMAN_DIR")) {
        std::filesystem::path sdkmanJavaPath(sdkmanDir);
        sdkmanJavaPath /= "candidates";
        sdkmanJavaPath /= "java";
        std::error_code ec;
        if (std::filesystem::is_directory(sdkmanJavaPath, ec) && !ec) {
            for (const auto& entry : std::filesystem::directory_iterator(sdkmanJavaPath, ec)) {
                if (ec || !entry.is_directory()) {
                    continue;
                }
                push_if_exists(&candidates, entry.path() / "bin" / javaName);
            }
        }
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

std::string infer_vendor_from_path(const std::filesystem::path& executable) {
    const auto text = executable.generic_string();
    if (text.find("Adoptium") != std::string::npos || text.find("temurin") != std::string::npos ||
        text.find("Temurin") != std::string::npos) {
        return "adoptium";
    }
    if (text.find("Microsoft") != std::string::npos) {
        return "microsoft";
    }
    if (text.find("BellSoft") != std::string::npos || text.find("liberica") != std::string::npos) {
        return "bellsoft";
    }
    if (text.find("Oracle") != std::string::npos) {
        return "oracle";
    }
    if (text.find("Corretto") != std::string::npos || text.find("Amazon") != std::string::npos) {
        return "amazon";
    }
    if (text.find("Zulu") != std::string::npos || text.find("Azul") != std::string::npos) {
        return "azul";
    }
    if (text.find("openjdk") != std::string::npos || text.find("OpenJDK") != std::string::npos) {
        return "openjdk";
    }
    if (text.find("jdk") != std::string::npos || text.find("Java") != std::string::npos) {
        return "system";
    }
    return "unknown";
}

std::string get_current_os() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "mac";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string get_current_arch() {
#if defined(_WIN32)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64) {
        return "x64";
    } else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64) {
        return "aarch64";
    } else if (si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_INTEL) {
        return "x86";
    }
#elif defined(__APPLE__) || defined(__linux__)
    utsname buf;
    if (uname(&buf) == 0) {
        std::string machine(buf.machine);
        if (machine == "x86_64" || machine == "amd64") {
            return "x64";
        } else if (machine == "aarch64" || machine == "arm64") {
            return "aarch64";
        } else if (machine == "i386" || machine == "i686") {
            return "x86";
        }
    }
#endif
    // Default fallback
#if defined(__x86_64__) || defined(_M_X64)
    return "x64";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "x64";
#endif
}

// Extract archive based on extension
bool extract_archive(const std::filesystem::path& archivePath, const std::filesystem::path& outputDir, std::string* error) {
    std::string ext = archivePath.extension().string();
    std::string stemExt = archivePath.stem().extension().string();

    // Convert to lowercase for comparison
    auto toLower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
        return s;
    };

    ext = toLower(ext);
    stemExt = toLower(stemExt);

    bool isZip = (ext == ".zip");
    bool isTarGz = (ext == ".gz" && stemExt == ".tar") || (ext == ".tgz");
    bool isTar = (ext == ".tar");

    if (!isZip && !isTarGz && !isTar) {
        if (error) *error = "Unsupported archive format: " + archivePath.string();
        return false;
    }

#if defined(_WIN32)
    // On Windows, use PowerShell for extraction
    std::string command;
    if (isZip) {
        command = "powershell -Command \"Expand-Archive -Path '\"" + archivePath.string() + "\"' -DestinationPath '\"" + outputDir.string() + "\"' -Force\"";
    } else {
        // For tar.gz on Windows, also use PowerShell
        command = "powershell -Command \"tar -xzf '\"" + archivePath.string() + "\"' -C '\"" + outputDir.string() + "\"'\"";
    }

    int result = system(command.c_str());
    if (result != 0) {
        if (error) *error = "Failed to extract archive using PowerShell";
        return false;
    }
#else
    std::string command;
    if (isZip) {
        command = "unzip -q -o \"" + archivePath.string() + "\" -d \"" + outputDir.string() + "\"";
    } else if (isTarGz) {
        command = "tar -xzf \"" + archivePath.string() + "\" -C \"" + outputDir.string() + "\"";
    } else {
        command = "tar -xf \"" + archivePath.string() + "\" -C \"" + outputDir.string() + "\"";
    }

    int result = system(command.c_str());
    if (result != 0) {
        if (error) *error = "Failed to extract archive";
        return false;
    }
#endif

    return true;
}

// Find Java executable in extracted directory
std::filesystem::path find_java_in_directory(const std::filesystem::path& dir) {
    std::error_code ec;
    auto javaName = java_binary_name();

    // Common subdirectories to check
    std::vector<std::filesystem::path> pathsToCheck = {
        dir / "bin" / javaName,
        dir / javaName,
    };

    // Search one level deep for nested structures
    if (std::filesystem::is_directory(dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (ec || !entry.is_directory()) {
                continue;
            }
            pathsToCheck.push_back(entry.path() / "bin" / javaName);
            pathsToCheck.push_back(entry.path() / javaName);
        }
    }

    for (const auto& path : pathsToCheck) {
        if (std::filesystem::exists(path, ec) && !ec) {
            return path;
        }
    }

    return {};
}

} // namespace

// Static method to check if Java executable works and get its info
bool JavaService::check_java_executable(const std::filesystem::path& executable, JavaRuntimeInfo* info) {
    std::error_code ec;
    if (!std::filesystem::exists(executable, ec) || ec) {
        return false;
    }

    auto [exitCode, output] = execute_java_version(executable);
    if (exitCode != 0) {
        return false;
    }

    if (info != nullptr) {
        std::string fullVersion;
        std::string vendor;
        int majorVersion = parse_java_version_output(output, fullVersion, vendor);

        info->executable = executable;
        info->versionText = fullVersion.empty() ? "unknown" : fullVersion;
        info->majorVersion = majorVersion;
        info->vendor = vendor;
        info->available = true;

        // If vendor wasn't detected from version output, try path
        if (vendor == "unknown") {
            info->vendor = infer_vendor_from_path(executable);
        }
    }

    return true;
}

// Static method to get recommended Java version for Minecraft version
int JavaService::recommended_java_version(const std::string& mcVersion) {
    if (mcVersion.empty()) {
        return 17; // Default for modern Minecraft
    }

    // Parse version components
    std::regex versionRegex(R"((\d+)\.(\d+)(?:\.(\d+))?")");
    std::smatch match;

    if (!std::regex_search(mcVersion, match, versionRegex)) {
        return 17; // Default
    }

    int major = std::stoi(match[1].str());
    int minor = std::stoi(match[2].str());
    int patch = 0;
    if (match[3].matched) {
        patch = std::stoi(match[3].str());
    }

    // Minecraft 1.20.5+ requires Java 21
    if (major == 1 && minor == 20 && patch >= 5) {
        return 21;
    }
    if (major == 1 && minor >= 21) {
        return 21;
    }

    // Minecraft 1.18+ requires Java 17
    if (major == 1 && minor >= 18) {
        return 17;
    }

    // Minecraft 1.17-1.17.1 requires Java 16/17
    if (major == 1 && minor == 17) {
        return 17;
    }

    // Minecraft 1.16.5 and below uses Java 8
    if (major == 1 && minor <= 16) {
        return 8;
    }

    return 17; // Default fallback
}

JavaService::JavaService()
    : httpClient_(dawn::infra::net::HttpClientFactory::create_default_http_client()) {
}

JavaService::JavaService(std::shared_ptr<dawn::infra::net::HttpClient> httpClient)
    : httpClient_(httpClient ? std::move(httpClient) : dawn::infra::net::HttpClientFactory::create_default_http_client()) {
}

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

        // Actually check if Java works and get version info
        JavaRuntimeInfo runtime;
        if (check_java_executable(candidate, &runtime)) {
            runtime.id = "java-runtime-" + std::to_string(++index);
            runtime.bundled = false;
            runtimes.push_back(std::move(runtime));
        }
    }

    // Sort by major version (descending) for better recommendations
    std::sort(runtimes.begin(), runtimes.end(), [](const JavaRuntimeInfo& a, const JavaRuntimeInfo& b) {
        if (a.majorVersion != b.majorVersion) {
            return a.majorVersion > b.majorVersion;
        }
        return a.vendor < b.vendor;
    });

    // Reassign IDs after sorting
    for (size_t i = 0; i < runtimes.size(); ++i) {
        runtimes[i].id = "java-runtime-" + std::to_string(i + 1);
    }

    return runtimes;
}

JavaRuntimeInfo JavaService::recommended_runtime(const std::string& mcVersion) const {
    const auto runtimes = discover_runtimes();
    int requiredVersion = recommended_java_version(mcVersion);

    // Find the best matching runtime
    const JavaRuntimeInfo* bestMatch = nullptr;
    const JavaRuntimeInfo* anyCompatible = nullptr;

    for (const auto& runtime : runtimes) {
        if (!runtime.available) {
            continue;
        }

        // Exact version match is preferred
        if (runtime.majorVersion == requiredVersion) {
            if (bestMatch == nullptr || runtime.vendor == "adoptium" || runtime.vendor == "microsoft") {
                bestMatch = &runtime;
            }
        }

        // Check if compatible (higher version that can run lower version code)
        if (runtime.majorVersion >= requiredVersion) {
            if (anyCompatible == nullptr) {
                anyCompatible = &runtime;
            }
        }
    }

    if (bestMatch != nullptr) {
        return *bestMatch;
    }

    if (anyCompatible != nullptr) {
        return *anyCompatible;
    }

    if (!runtimes.empty()) {
        return runtimes.front();
    }

    // Return fallback if no Java found
    return JavaRuntimeInfo{
        .id = "fallback-java",
        .executable = std::filesystem::path("java"),
        .vendor = "fallback",
        .versionText = "unknown",
        .majorVersion = requiredVersion,
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

bool JavaService::is_compatible(const JavaRuntimeInfo& runtime, int minMajorVersion) const {
    if (!runtime.available) {
        return false;
    }
    if (runtime.majorVersion == 0) {
        return true;
    }
    return runtime.majorVersion >= minMajorVersion;
}

JavaDownloadResult JavaService::download_java(int majorVersion, const std::filesystem::path& outputDir) const {
    JavaDownloadOptions options;
    options.majorVersion = majorVersion;
    options.os = get_current_os();
    options.arch = get_current_arch();
    return download_java(options, outputDir);
}

JavaDownloadResult JavaService::download_java(const JavaDownloadOptions& options, const std::filesystem::path& outputDir) const {
    JavaDownloadResult result;

    if (!httpClient_) {
        result.error = "HTTP client not available";
        return result;
    }

    // Build Adoptium API URL
    // https://api.adoptium.net/v3/binary/latest/{feature_version}/ga/{os}/{arch}/jdk/hotspot/normal/eclipse
    std::string os = options.os.empty() ? get_current_os() : options.os;
    std::string arch = options.arch.empty() ? get_current_arch() : options.arch;

    // Normalize architecture names for Adoptium API
    if (arch == "x86_64" || arch == "amd64") {
        arch = "x64";
    } else if (arch == "arm64") {
        arch = "aarch64";
    }

    // Normalize OS names for Adoptium API
    if (os == "macos" || os == "darwin") {
        os = "mac";
    }

    std::stringstream urlBuilder;
    urlBuilder << "https://api.adoptium.net/v3/binary/latest/"
               << options.majorVersion << "/"
               << options.releaseType << "/"
               << os << "/"
               << arch << "/"
               << options.imageType << "/"
               << "hotspot/normal/"
               << options.vendor;

    std::string downloadUrl = urlBuilder.str();

    // Create temporary download path
    std::error_code ec;
    if (!std::filesystem::exists(outputDir, ec)) {
        std::string mkdirError;
        if (!dawn::infra::fs::ensure_directory(outputDir, &mkdirError)) {
            result.error = "Failed to create output directory: " + mkdirError;
            return result;
        }
    }

    // Determine file extension based on OS
    std::string ext = (os == "windows") ? ".zip" : ".tar.gz";
    std::filesystem::path tempFile = outputDir / ("java-" + std::to_string(options.majorVersion) + "-download" + ext);

    // Download the file
    dawn::infra::net::HttpRequest request;
    request.method = dawn::infra::net::HttpMethod::Get;
    request.url = downloadUrl;
    request.headers["Accept"] = "application/octet-stream";
    request.headers["User-Agent"] = "DawnLauncher/1.0";

    auto response = httpClient_->send(request);

    if (!response.success()) {
        result.error = "Download failed with HTTP status: " + std::to_string(response.statusCode);
        return result;
    }

    // Save to file
    std::string writeError;
    if (!dawn::infra::fs::write_binary_file(tempFile, response.body, &writeError)) {
        result.error = "Failed to save download: " + writeError;
        return result;
    }

    // Extract archive
    std::filesystem::path extractDir = outputDir / ("jdk-" + std::to_string(options.majorVersion));
    std::string extractError;
    if (!extract_archive(tempFile, extractDir, &extractError)) {
        result.error = "Failed to extract archive: " + extractError;
        std::filesystem::remove(tempFile, ec);
        return result;
    }

    // Clean up archive
    std::filesystem::remove(tempFile, ec);

    // Find Java executable in extracted directory
    auto javaExecutable = find_java_in_directory(extractDir);
    if (javaExecutable.empty()) {
        result.error = "Could not find Java executable in extracted directory";
        return result;
    }

    // Verify the downloaded Java works
    JavaRuntimeInfo runtimeInfo;
    if (!check_java_executable(javaExecutable, &runtimeInfo)) {
        result.error = "Downloaded Java executable is not working properly";
        return result;
    }

    // Ensure the runtime has the correct major version
    if (runtimeInfo.majorVersion != options.majorVersion) {
        result.error = "Downloaded Java version (" + std::to_string(runtimeInfo.majorVersion) +
                      ") does not match requested version (" + std::to_string(options.majorVersion) + ")";
        return result;
    }

    result.success = true;
    result.extractedPath = extractDir;
    result.runtimeInfo = runtimeInfo;

    return result;
}

} // namespace dawn::core
