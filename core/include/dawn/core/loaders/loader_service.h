#pragma once

#include "dawn/core/model/enums.h"

#include <string>
#include <vector>

namespace dawn::core {

struct LoaderProfile {
    LoaderType loaderType = LoaderType::None;
    std::string versionId;
    std::string mcVersion;
    std::string notes;
    std::string javaHint;
};

class LoaderService {
public:
    std::vector<LoaderProfile> list_loaders(const std::string& mcVersion) const;
    LoaderProfile recommend_loader(const std::string& mcVersion, LoaderType preferred = LoaderType::None) const;
};

} // namespace dawn::core
