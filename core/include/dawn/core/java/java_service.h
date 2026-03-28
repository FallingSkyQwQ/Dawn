#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dawn::infra::net {
class HttpClient;
}

namespace dawn::core {

struct JavaRuntimeInfo {
    std::string id;
    std::filesystem::path executable;
    std::string vendor;
    std::string versionText;
    int majorVersion = 0;
    bool bundled = false;
    bool available = true;
};

struct JavaProfile {
    std::string profileId;
    std::string runtimeId;
    int minimumMajor = 17;
    int maximumMajor = 21;
    bool autoDiscover = true;
    std::string notes;
};

struct JavaDownloadOptions {
    int majorVersion = 17;
    std::string os;
    std::string arch;
    std::string vendor = "eclipse";
    std::string imageType = "jdk";
    std::string releaseType = "ga";
};

struct JavaDownloadResult {
    bool success = false;
    std::string error;
    std::filesystem::path extractedPath;
    JavaRuntimeInfo runtimeInfo;
};

class JavaService {
public:
    JavaService();
    explicit JavaService(std::shared_ptr<dawn::infra::net::HttpClient> httpClient);

    // Discover installed Java runtimes
    std::vector<JavaRuntimeInfo> discover_runtimes() const;

    // Get recommended runtime for a specific Minecraft version
    JavaRuntimeInfo recommended_runtime(const std::string& mcVersion = "") const;

    // Build a Java profile from runtime info
    JavaProfile build_profile(const JavaRuntimeInfo& runtime) const;

    // Check if runtime is compatible with profile
    bool is_compatible(const JavaProfile& profile, const JavaRuntimeInfo& runtime) const;

    // Check if runtime meets minimum major version requirement
    bool is_compatible(const JavaRuntimeInfo& runtime, int minMajorVersion) const;

    // Download Java from Adoptium API
    JavaDownloadResult download_java(int majorVersion, const std::filesystem::path& outputDir) const;
    JavaDownloadResult download_java(const JavaDownloadOptions& options, const std::filesystem::path& outputDir) const;

    // Get recommended Java major version for Minecraft version
    static int recommended_java_version(const std::string& mcVersion);

    // Check if a Java runtime is available and working
    static bool check_java_executable(const std::filesystem::path& executable, JavaRuntimeInfo* info = nullptr);

private:
    std::shared_ptr<dawn::infra::net::HttpClient> httpClient_;
};

} // namespace dawn::core
