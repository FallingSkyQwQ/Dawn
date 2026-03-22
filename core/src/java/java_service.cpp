#include "dawn/core/java/java_service.h"

#include <cstdlib>

namespace dawn::core {

std::vector<JavaRuntimeInfo> JavaService::discover_runtimes() const {
    std::vector<JavaRuntimeInfo> runtimes;

    if (const char* java_home = std::getenv("JAVA_HOME")) {
        JavaRuntimeInfo home;
        home.id = "java-home";
        home.executable = std::filesystem::path(java_home) / "bin" / "java";
#if defined(_WIN32)
        home.executable = home.executable.replace_extension(".exe");
#endif
        home.vendor = "environment";
        home.versionText = "unknown";
        home.majorVersion = 17;
        home.available = true;
        runtimes.push_back(std::move(home));
    }

    JavaRuntimeInfo bundled;
    bundled.id = "bundled-stub";
    bundled.executable = std::filesystem::path("java");
#if defined(_WIN32)
    bundled.executable = bundled.executable.replace_extension(".exe");
#endif
    bundled.vendor = "dawn";
    bundled.versionText = "17-stub";
    bundled.majorVersion = 17;
    bundled.bundled = true;
    runtimes.push_back(std::move(bundled));

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
