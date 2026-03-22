#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dawn::core {

struct ContentLock {
    std::string provider;
    std::string projectId;
    std::string versionId;
    std::string fileHash;
    std::filesystem::path installedPath;
    bool enabled = true;
    std::vector<std::string> dependencies;
};

} // namespace dawn::core
