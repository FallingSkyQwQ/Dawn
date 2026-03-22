#pragma once

#include <filesystem>
#include <string>
#include <vector>

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

class JavaService {
public:
    std::vector<JavaRuntimeInfo> discover_runtimes() const;
    JavaRuntimeInfo recommended_runtime() const;
    JavaProfile build_profile(const JavaRuntimeInfo& runtime) const;
    bool is_compatible(const JavaProfile& profile, const JavaRuntimeInfo& runtime) const;
};

} // namespace dawn::core
