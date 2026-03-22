#include "dawn/core/provider/modrinth_provider.h"

#include "dawn/infra/net/http_client_factory.h"
#include "dawn/infra/json/simple_json.h"

#include <cstddef>
#include <filesystem>
#include <algorithm>
#include <memory>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;
using dawn::infra::net::HttpMethod;
using dawn::infra::net::HttpRequest;
using dawn::infra::net::HttpResponse;

constexpr const char* kBaseUrl = "https://api.modrinth.com/v2";

std::vector<std::string> loader_strings(const std::vector<LoaderType>& loaders) {
    std::vector<std::string> result;
    result.reserve(loaders.size());
    for (const auto loader : loaders) {
        result.emplace_back(to_string(loader));
    }
    return result;
}

std::vector<std::string> loader_strings_from_categories(const std::vector<std::string>& categories) {
    std::vector<std::string> result;
    result.reserve(categories.size());
    for (const auto& category : categories) {
        const auto loader = loader_type_from_string(category);
        if (loader != LoaderType::None) {
            result.emplace_back(to_string(loader));
        }
    }
    return result;
}

std::vector<std::string> read_string_array(const Value* value) {
    std::vector<std::string> result;
    if (!value || !value->is_array()) {
        return result;
    }
    for (const auto& entry : value->as_array()) {
        if (entry.is_string()) {
            result.push_back(entry.as_string());
        }
    }
    return result;
}

bool read_string(const Value::Object& object, const std::string& key, std::string* out) {
    const auto* value = dawn::infra::json::find(object, key);
    if (!value || !value->is_string()) {
        return false;
    }
    if (out) {
        *out = value->as_string();
    }
    return true;
}

bool read_int(const Value::Object& object, const std::string& key, int* out) {
    const auto* value = dawn::infra::json::find(object, key);
    if (!value || !value->is_number()) {
        return false;
    }
    if (out) {
        *out = static_cast<int>(value->as_number());
    }
    return true;
}

void add_facet(Value::Array& facets, const std::string& key, const std::vector<std::string>& values) {
    if (values.empty()) {
        return;
    }
    Value::Array entry;
    entry.reserve(values.size());
    for (const auto& value : values) {
        entry.emplace_back(key + ":" + value);
    }
    facets.emplace_back(std::move(entry));
}

std::string facets_to_query_value(const SearchQuery& query) {
    Value::Array facets;
    add_facet(facets, "project_type", {std::string(to_string(query.projectType))});
    add_facet(facets, "categories", query.categories);
    add_facet(facets, "categories", loader_strings(query.loaders));
    add_facet(facets, "versions", query.gameVersions);
    if (query.clientSide) {
        add_facet(facets, "client_side", {"required"});
    }
    if (query.serverSide) {
        add_facet(facets, "server_side", {"required"});
    }
    if (facets.empty()) {
        return {};
    }
    return dawn::infra::json::stringify(Value(std::move(facets)), 0);
}

std::string array_query_value(const std::vector<std::string>& values) {
    Value::Array array;
    array.reserve(values.size());
    for (const auto& value : values) {
        array.emplace_back(value);
    }
    return dawn::infra::json::stringify(Value(std::move(array)), 0);
}

SearchResult fallback_search_result(const SearchQuery& query) {
    SearchResult result;

    SearchResultItem item;
    item.projectId = "dawn-demo-project";
    item.title = query.text.empty() ? "Dawn Demo Project" : "Dawn Match: " + query.text;
    item.summary = "Local provider stub for Modrinth-style search.";
    item.author = "Dawn";
    item.updatedAt = "2026-03-22T00:00:00Z";
    item.downloads = 1024;
    item.projectType = query.projectType;
    item.supportedGameVersions = query.gameVersions.empty() ? std::vector<std::string>{"1.20.1"} : query.gameVersions;
    const auto combined_loaders = !query.loaders.empty() ? loader_strings(query.loaders) : loader_strings_from_categories(query.categories);
    if (combined_loaders.empty()) {
        item.supportedLoaders = {LoaderType::Fabric};
    } else {
        for (const auto& loader_name : combined_loaders) {
            const auto loader = loader_type_from_string(loader_name);
            if (loader != LoaderType::None) {
                item.supportedLoaders.push_back(loader);
            }
        }
    }
    result.items.push_back(std::move(item));
    return result;
}

std::vector<ContentVersion> fallback_versions(const std::string& projectId) {
    ContentVersion version;
    version.versionId = projectId + "-v1";
    version.name = "Local stub build";
    version.fileUrls = {"https://example.invalid/" + projectId + ".jar"};
    version.dependencies = {"dawn-core"};
    version.gameVersions = {"1.20.1"};
    version.loaders = {LoaderType::Fabric, LoaderType::Forge};
    return {std::move(version)};
}

SearchResultItem parse_search_hit(const Value::Object& object, const SearchQuery& fallbackQuery) {
    SearchResultItem item;
    if (const auto* value = dawn::infra::json::find(object, "project_id"); value && value->is_string()) item.projectId = value->as_string();
    if (item.projectId.empty()) {
        read_string(object, "projectId", &item.projectId);
    }
    if (const auto* value = dawn::infra::json::find(object, "title"); value && value->is_string()) item.title = value->as_string();
    if (item.title.empty()) {
        read_string(object, "slug", &item.title);
    }
    if (const auto* value = dawn::infra::json::find(object, "description"); value && value->is_string()) item.summary = value->as_string();
    if (const auto* value = dawn::infra::json::find(object, "author"); value && value->is_string()) item.author = value->as_string();
    if (item.author.empty()) {
        read_string(object, "author_username", &item.author);
    }
    if (const auto* value = dawn::infra::json::find(object, "icon_url"); value && value->is_string()) item.iconUrl = value->as_string();
    if (item.iconUrl.empty()) {
        read_string(object, "iconUrl", &item.iconUrl);
    }
    if (const auto* value = dawn::infra::json::find(object, "date_modified"); value && value->is_string()) item.updatedAt = value->as_string();
    if (item.updatedAt.empty()) {
        read_string(object, "updated", &item.updatedAt);
    }
    if (const auto* value = dawn::infra::json::find(object, "downloads"); value && value->is_number()) item.downloads = static_cast<std::size_t>(value->as_number());
    if (const auto* value = dawn::infra::json::find(object, "project_type"); value && value->is_string()) {
        item.projectType = project_type_from_string(value->as_string());
    } else {
        item.projectType = fallbackQuery.projectType;
    }
    if (const auto* value = dawn::infra::json::find(object, "versions"); value && value->is_array()) {
        item.supportedGameVersions = read_string_array(value);
    } else if (const auto* value = dawn::infra::json::find(object, "game_versions"); value && value->is_array()) {
        item.supportedGameVersions = read_string_array(value);
    } else {
        item.supportedGameVersions = fallbackQuery.gameVersions;
    }

    std::vector<std::string> categories;
    if (const auto* value = dawn::infra::json::find(object, "categories"); value && value->is_array()) {
        categories = read_string_array(value);
    }
    if (const auto* value = dawn::infra::json::find(object, "loaders"); value && value->is_array()) {
        const auto loaders = read_string_array(value);
        categories.insert(categories.end(), loaders.begin(), loaders.end());
    }
    for (const auto& category : categories) {
        const auto loader = loader_type_from_string(category);
        if (loader != LoaderType::None) {
            item.supportedLoaders.push_back(loader);
        }
    }
    if (item.supportedLoaders.empty()) {
        item.supportedLoaders = fallbackQuery.loaders;
    }
    return item;
}

ContentVersion parse_version_entry(const Value::Object& object) {
    ContentVersion version;
    if (const auto* value = dawn::infra::json::find(object, "id"); value && value->is_string()) version.versionId = value->as_string();
    if (version.versionId.empty()) {
        read_string(object, "version_id", &version.versionId);
    }
    if (const auto* value = dawn::infra::json::find(object, "name"); value && value->is_string()) version.name = value->as_string();
    if (version.name.empty()) {
        read_string(object, "version_number", &version.name);
    }
    if (const auto* value = dawn::infra::json::find(object, "game_versions"); value && value->is_array()) {
        version.gameVersions = read_string_array(value);
    }
    if (const auto* value = dawn::infra::json::find(object, "loaders"); value && value->is_array()) {
        const auto loaderNames = read_string_array(value);
        for (const auto& loaderName : loaderNames) {
            const auto loader = loader_type_from_string(loaderName);
            if (loader != LoaderType::None) {
                version.loaders.push_back(loader);
            }
        }
    }
    if (const auto* value = dawn::infra::json::find(object, "files"); value && value->is_array()) {
        for (const auto& file : value->as_array()) {
            if (!file.is_object()) {
                continue;
            }
            const auto& fileObject = file.as_object();
            const auto* url = dawn::infra::json::find(fileObject, "url");
            if (url && url->is_string()) {
                version.fileUrls.push_back(url->as_string());
            }
        }
    }
    if (const auto* value = dawn::infra::json::find(object, "dependencies"); value && value->is_array()) {
        for (const auto& dependency : value->as_array()) {
            if (!dependency.is_object()) {
                continue;
            }
            const auto& dependencyObject = dependency.as_object();
            if (const auto* entry = dawn::infra::json::find(dependencyObject, "version_id"); entry && entry->is_string()) {
                version.dependencies.push_back(entry->as_string());
            } else if (const auto* entry = dawn::infra::json::find(dependencyObject, "project_id"); entry && entry->is_string()) {
                version.dependencies.push_back(entry->as_string());
            }
        }
    }
    return version;
}

SearchResult parse_search_response(const HttpResponse& response, const SearchQuery& query, bool* ok) {
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        if (ok) {
            *ok = false;
        }
        return {};
    }

    const auto& object = parsed.value.as_object();
    const auto* hits = dawn::infra::json::find(object, "hits");
    if (!hits || !hits->is_array()) {
        if (ok) {
            *ok = false;
        }
        return {};
    }

    SearchResult result;
    for (const auto& entry : hits->as_array()) {
        if (!entry.is_object()) {
            continue;
        }
        result.items.push_back(parse_search_hit(entry.as_object(), query));
    }
    if (const auto* value = dawn::infra::json::find(object, "next_cursor"); value && value->is_string()) {
        result.nextCursor = value->as_string();
    } else if (const auto* value = dawn::infra::json::find(object, "cursor"); value && value->is_string()) {
        result.nextCursor = value->as_string();
    }
    if (ok) {
        *ok = true;
    }
    return result;
}

std::vector<ContentVersion> parse_versions_response(const HttpResponse& response, bool* ok) {
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_array()) {
        if (ok) {
            *ok = false;
        }
        return {};
    }

    std::vector<ContentVersion> result;
    for (const auto& entry : parsed.value.as_array()) {
        if (entry.is_object()) {
            result.push_back(parse_version_entry(entry.as_object()));
        }
    }
    if (ok) {
        *ok = true;
    }
    return result;
}

HttpRequest make_get_request(const std::string& url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = url;
    request.headers.emplace("Accept", "application/json");
    return request;
}

} // namespace

ModrinthProvider::ModrinthProvider(std::shared_ptr<dawn::infra::net::HttpClient> client)
    : client_(client ? std::move(client) : dawn::infra::net::HttpClientFactory::create_default_http_client()) {}

const std::shared_ptr<dawn::infra::net::HttpClient>& ModrinthProvider::http_client() const noexcept {
    return client_;
}

std::string ModrinthProvider::build_search_facets(const SearchQuery& query) {
    return facets_to_query_value(query);
}

std::string ModrinthProvider::build_search_url(const SearchQuery& query) {
    std::vector<std::pair<std::string, std::string>> params;
    params.emplace_back("query", query.text);
    params.emplace_back("index", "relevance");
    params.emplace_back("limit", "20");
    params.emplace_back("offset", "0");
    const auto facets = build_search_facets(query);
    if (!facets.empty()) {
        params.emplace_back("facets", facets);
    }
    return dawn::infra::net::append_query(std::string(kBaseUrl) + "/search", params);
}

std::string ModrinthProvider::build_versions_url(const std::string& projectId, const ModrinthVersionQuery& query) {
    std::vector<std::pair<std::string, std::string>> params;
    params.emplace_back("include_changelog", query.includeChangelog ? "true" : "false");
    if (query.featured.has_value()) {
        params.emplace_back("featured", *query.featured ? "true" : "false");
    }
    if (!query.loaders.empty()) {
        params.emplace_back("loaders", array_query_value(query.loaders));
    }
    if (!query.gameVersions.empty()) {
        params.emplace_back("game_versions", array_query_value(query.gameVersions));
    }
    return dawn::infra::net::append_query(std::string(kBaseUrl) + "/project/" + dawn::infra::net::url_encode(projectId) + "/version", params);
}

SearchResult ModrinthProvider::search(const SearchQuery& query) {
    if (!client_) {
        return fallback_search_result(query);
    }

    const auto request = make_get_request(build_search_url(query));
    const auto response = client_->send(request);
    if (!response.success()) {
        return fallback_search_result(query);
    }

    bool ok = false;
    const auto parsed = parse_search_response(response, query, &ok);
    if (!ok) {
        return fallback_search_result(query);
    }
    return parsed;
}

std::vector<ContentVersion> ModrinthProvider::versions(const std::string& projectId) {
    return versions(projectId, ModrinthVersionQuery{});
}

std::vector<ContentVersion> ModrinthProvider::versions(const std::string& projectId, const ModrinthVersionQuery& query) {
    if (!client_) {
        return fallback_versions(projectId);
    }

    const auto request = make_get_request(build_versions_url(projectId, query));
    const auto response = client_->send(request);
    if (!response.success()) {
        return fallback_versions(projectId);
    }

    bool ok = false;
    const auto parsed = parse_versions_response(response, &ok);
    if (!ok) {
        return fallback_versions(projectId);
    }
    return parsed;
}

DependencyGraph ModrinthProvider::resolveDependencies(const InstallRequest& request) {
    DependencyGraph graph;
    ContentLock lock;
    lock.provider = "modrinth";
    lock.projectId = request.projectId;
    lock.versionId = request.versionId;
    lock.fileHash = "stub-hash";
    lock.installedPath = std::filesystem::path("mods") / (request.projectId + ".jar");
    lock.enabled = true;
    lock.dependencies = {"minecraft"};
    graph.locks.push_back(std::move(lock));
    graph.missing.push_back("java");
    return graph;
}

TaskPlan ModrinthProvider::buildInstallPlan(const InstallRequest& request) {
    TaskPlan plan;
    plan.id = request.projectId + "-" + request.versionId;
    plan.title = "Install " + request.projectId;
    plan.steps = {
        {"resolve", "Resolve dependencies", TaskStatus::Pending, 0, {}},
        {"download", "Download artifacts", TaskStatus::Pending, 0, {}},
        {"verify", "Verify hashes", TaskStatus::Pending, 0, {}},
        {"install", "Install into instance", TaskStatus::Pending, 0, {}}
    };
    return plan;
}

} // namespace dawn::core
