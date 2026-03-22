#include "dawn/core/loaders/loader_service.h"

namespace dawn::core {

std::vector<LoaderProfile> LoaderService::list_loaders(const std::string& mcVersion) const {
    return {
        {LoaderType::Fabric, mcVersion + "-fabric", mcVersion, "fast modern loader", "17"},
        {LoaderType::Quilt, mcVersion + "-quilt", mcVersion, "fabric-compatible fork", "17"},
        {LoaderType::Forge, mcVersion + "-forge", mcVersion, "classic modding stack", "17"},
        {LoaderType::NeoForge, mcVersion + "-neoforge", mcVersion, "new forge line", "17"},
        {LoaderType::OptiFine, mcVersion + "-optifine", mcVersion, "special-case adapter", "17"},
    };
}

LoaderProfile LoaderService::recommend_loader(const std::string& mcVersion, LoaderType preferred) const {
    const auto loaders = list_loaders(mcVersion);
    if (preferred != LoaderType::None) {
        for (const auto& loader : loaders) {
            if (loader.loaderType == preferred) {
                return loader;
            }
        }
    }
    return loaders.front();
}

} // namespace dawn::core
