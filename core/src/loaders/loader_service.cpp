#include "dawn/core/loaders/loader_service.h"

#include "dawn/infra/json/simple_json.h"
#include "dawn/infra/net/http_client_factory.h"

#include <algorithm>
#include <regex>
#include <string_view>
#include <vector>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;
using dawn::infra::net::HttpRequest;

std::string read_string_field(const Value::Object& object, const std::string& key) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_string()) {
        return value->as_string();
    }
    return {};
}

std::string to_neoforge_family(const std::string& mcVersion) {
    if (mcVersion.rfind("1.", 0) == 0) {
        return mcVersion.substr(2);
    }
    return mcVersion;
}

std::string fetch_text(const std::string& url) {
    const auto client = dawn::infra::net::HttpClientFactory::create_default_http_client();
    if (!client) {
        return {};
    }
    HttpRequest request;
    request.url = url;
    request.headers.emplace("Accept", "application/json");
    const auto response = client->send(request);
    if (!response.success()) {
        return {};
    }
    return response.body;
}

std::vector<std::string> fetch_fabric_versions(const std::string& mcVersion) {
    const auto body = fetch_text("https://meta.fabricmc.net/v2/versions/loader/" + mcVersion);
    if (body.empty()) {
        return {};
    }
    const auto parsed = dawn::infra::json::parse(body);
    if (!parsed.ok || !parsed.value.is_array()) {
        return {};
    }
    std::vector<std::string> result;
    for (const auto& entry : parsed.value.as_array()) {
        if (!entry.is_object()) {
            continue;
        }
        const auto* loader = dawn::infra::json::find(entry.as_object(), "loader");
        if (!loader || !loader->is_object()) {
            continue;
        }
        const auto version = read_string_field(loader->as_object(), "version");
        if (!version.empty()) {
            result.push_back(version);
        }
    }
    return result;
}

std::vector<std::string> fetch_quilt_versions(const std::string& mcVersion) {
    const auto body = fetch_text("https://meta.quiltmc.org/v3/versions/loader/" + mcVersion);
    if (body.empty()) {
        return {};
    }
    const auto parsed = dawn::infra::json::parse(body);
    if (!parsed.ok || !parsed.value.is_array()) {
        return {};
    }
    std::vector<std::string> result;
    for (const auto& entry : parsed.value.as_array()) {
        if (!entry.is_object()) {
            continue;
        }
        const auto* loader = dawn::infra::json::find(entry.as_object(), "loader");
        if (!loader || !loader->is_object()) {
            continue;
        }
        const auto version = read_string_field(loader->as_object(), "version");
        if (!version.empty()) {
            result.push_back(version);
        }
    }
    return result;
}

std::string fetch_forge_version(const std::string& mcVersion) {
    const auto body = fetch_text("https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json");
    if (body.empty()) {
        return {};
    }
    const auto parsed = dawn::infra::json::parse(body);
    if (!parsed.ok || !parsed.value.is_object()) {
        return {};
    }
    const auto* promos = dawn::infra::json::find(parsed.value.as_object(), "promos");
    if (!promos || !promos->is_object()) {
        return {};
    }
    const auto recommended = read_string_field(promos->as_object(), mcVersion + "-recommended");
    if (!recommended.empty()) {
        return recommended;
    }
    return read_string_field(promos->as_object(), mcVersion + "-latest");
}

std::string fetch_neoforge_version(const std::string& mcVersion) {
    const auto body = fetch_text("https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml");
    if (body.empty()) {
        return {};
    }

    const auto family = to_neoforge_family(mcVersion);
    std::regex versionPattern(R"(<version>([^<]+)</version>)");
    std::sregex_iterator it(body.begin(), body.end(), versionPattern);
    std::sregex_iterator end;

    std::string best;
    for (; it != end; ++it) {
        const auto version = (*it)[1].str();
        if (version.rfind(family + ".", 0) == 0 || version == family) {
            best = version;
        }
    }
    return best;
}

void append_loader_profiles(
    std::vector<LoaderProfile>* out,
    LoaderType type,
    const std::string& mcVersion,
    const std::vector<std::string>& versions,
    const std::string& notes,
    const std::string& javaHint) {
    if (!out) {
        return;
    }
    for (const auto& version : versions) {
        out->push_back(LoaderProfile{type, version, mcVersion, notes, javaHint});
    }
}

} // namespace

std::vector<LoaderProfile> LoaderService::list_loaders(const std::string& mcVersion) const {
    std::vector<LoaderProfile> profiles;

    const auto fabric = fetch_fabric_versions(mcVersion);
    if (!fabric.empty()) {
        append_loader_profiles(&profiles, LoaderType::Fabric, mcVersion, fabric, "official Fabric loader metadata", "17");
    } else {
        profiles.push_back({LoaderType::Fabric, mcVersion + "-fabric", mcVersion, "fallback fabric profile", "17"});
    }

    const auto quilt = fetch_quilt_versions(mcVersion);
    if (!quilt.empty()) {
        append_loader_profiles(&profiles, LoaderType::Quilt, mcVersion, quilt, "official Quilt loader metadata", "17");
    } else {
        profiles.push_back({LoaderType::Quilt, mcVersion + "-quilt", mcVersion, "fallback quilt profile", "17"});
    }

    const auto forge = fetch_forge_version(mcVersion);
    profiles.push_back({
        LoaderType::Forge,
        forge.empty() ? mcVersion + "-forge" : forge,
        mcVersion,
        forge.empty() ? "fallback forge profile" : "forge promotions metadata",
        "17"
    });

    const auto neoforge = fetch_neoforge_version(mcVersion);
    profiles.push_back({
        LoaderType::NeoForge,
        neoforge.empty() ? mcVersion + "-neoforge" : neoforge,
        mcVersion,
        neoforge.empty() ? "fallback neoforge profile" : "neoforge maven metadata",
        "17"
    });

    profiles.push_back({LoaderType::OptiFine, mcVersion + "-optifine", mcVersion, "special-case adapter", "17"});
    return profiles;
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
