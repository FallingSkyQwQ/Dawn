#pragma once

#include "dawn/core/interfaces/content_provider.h"
#include "dawn/infra/net/http_client_factory.h"
#include "dawn/infra/net/http_client.h"

#include <memory>
#include <optional>

namespace dawn::core {

struct ModrinthVersionQuery {
    std::vector<std::string> loaders;
    std::vector<std::string> gameVersions;
    std::optional<bool> featured;
    bool includeChangelog = false;
};

class ModrinthProvider final : public IContentProvider {
public:
    explicit ModrinthProvider(std::shared_ptr<dawn::infra::net::HttpClient> client = {});

    [[nodiscard]] const std::shared_ptr<dawn::infra::net::HttpClient>& http_client() const noexcept;
    SearchResult search(const SearchQuery& query) override;
    std::vector<ContentVersion> versions(const std::string& projectId) override;
    std::vector<ContentVersion> versions(const std::string& projectId, const ModrinthVersionQuery& query);
    DependencyGraph resolveDependencies(const InstallRequest& request) override;
    TaskPlan buildInstallPlan(const InstallRequest& request) override;

    static std::string build_search_facets(const SearchQuery& query);
    static std::string build_search_url(const SearchQuery& query);
    static std::string build_versions_url(const std::string& projectId, const ModrinthVersionQuery& query);

private:
    std::shared_ptr<dawn::infra::net::HttpClient> client_;
};

} // namespace dawn::core
