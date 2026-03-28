#include "dawn/core/loaders/fabric_installer.h"

#include "dawn/infra/json/simple_json.h"
#include "dawn/infra/net/http_client_factory.h"

#include <chrono>
#include <format>
#include <sstream>

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

std::string get_current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::gmtime(&time_t_now), "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

} // namespace

struct FabricInstaller::Impl {
    static constexpr std::string_view META_API_BASE = "https://meta.fabricmc.net/v2";
    static constexpr std::string_view MAVEN_BASE = "https://maven.fabricmc.net";

    std::vector<LoaderVersion> parse_loader_versions(const std::string& mcVersion, const std::string& json) {
        std::vector<LoaderVersion> result;

        const auto parsed = dawn::infra::json::parse(json);
        if (!parsed.ok || !parsed.value.is_array()) {
            return result;
        }

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
                result.push_back(LoaderVersion{
                    .versionId = version,
                    .mcVersion = mcVersion,
                    .loaderType = LoaderType::Fabric
                });
            }
        }

        return result;
    }

    std::string build_loader_url(const std::string& mcVersion, const std::string& loaderVersion) {
        std::ostringstream oss;
        oss << MAVEN_BASE << "/net/fabricmc/fabric-loader/" << loaderVersion
            << "/fabric-loader-" << loaderVersion << ".jar";
        return oss.str();
    }

    std::string build_intermediary_url(const std::string& mcVersion) {
        std::ostringstream oss;
        oss << MAVEN_BASE << "/net/fabricmc/intermediary/" << mcVersion
            << "/intermediary-" << mcVersion << ".jar";
        return oss.str();
    }

    std::string build_launcher_meta_url(const std::string& mcVersion, const std::string& loaderVersion) {
        std::ostringstream oss;
        oss << META_API_BASE << "/versions/loader/" << mcVersion << "/" << loaderVersion << "/profile/json";
        return oss.str();
    }
};

FabricInstaller::FabricInstaller() : impl_(std::make_unique<Impl>()) {}

FabricInstaller::~FabricInstaller() = default;

std::vector<LoaderVersion> FabricInstaller::listVersions(const std::string& mcVersion) {
    std::ostringstream url;
    url << Impl::META_API_BASE << "/versions/loader/" << mcVersion;

    const auto body = fetch_text(url.str());
    if (body.empty()) {
        return {};
    }

    return impl_->parse_loader_versions(mcVersion, body);
}

TaskPlan FabricInstaller::buildInstallPlan(const LoaderInstallRequest& request) {
    TaskPlan plan;
    plan.id = std::format("fabric_install_{}_{}", request.instanceId, request.versionId);
    plan.title = std::format("Install Fabric {} for {}", request.versionId, request.mcVersion);
    plan.status = TaskStatus::Pending;
    plan.createdAt = get_current_timestamp();
    plan.updatedAt = plan.createdAt;

    // Step 1: Download Fabric Loader
    TaskStep downloadLoaderStep;
    downloadLoaderStep.id = std::format("{}_download_loader", plan.id);
    downloadLoaderStep.title = "Download Fabric Loader";
    downloadLoaderStep.status = TaskStatus::Pending;
    downloadLoaderStep.detail = impl_->build_loader_url(request.mcVersion, request.versionId);
    plan.steps.push_back(std::move(downloadLoaderStep));

    // Step 2: Download Intermediary mappings
    TaskStep downloadIntermediaryStep;
    downloadIntermediaryStep.id = std::format("{}_download_intermediary", plan.id);
    downloadIntermediaryStep.title = "Download Intermediary Mappings";
    downloadIntermediaryStep.status = TaskStatus::Pending;
    downloadIntermediaryStep.detail = impl_->build_intermediary_url(request.mcVersion);
    plan.steps.push_back(std::move(downloadIntermediaryStep));

    // Step 3: Fetch launcher metadata
    TaskStep fetchMetaStep;
    fetchMetaStep.id = std::format("{}_fetch_meta", plan.id);
    fetchMetaStep.title = "Fetch Launcher Metadata";
    fetchMetaStep.status = TaskStatus::Pending;
    fetchMetaStep.detail = impl_->build_launcher_meta_url(request.mcVersion, request.versionId);
    plan.steps.push_back(std::move(fetchMetaStep));

    // Step 4: Generate launch configuration
    TaskStep generateConfigStep;
    generateConfigStep.id = std::format("{}_generate_config", plan.id);
    generateConfigStep.title = "Generate Launch Configuration";
    generateConfigStep.status = TaskStatus::Pending;
    generateConfigStep.detail = std::format("instance:{}, loader:{}", request.instanceId, request.versionId);
    plan.steps.push_back(std::move(generateConfigStep));

    // Step 5: Install libraries
    TaskStep installLibsStep;
    installLibsStep.id = std::format("{}_install_libs", plan.id);
    installLibsStep.title = "Install Required Libraries";
    installLibsStep.status = TaskStatus::Pending;
    installLibsStep.detail = "Fabric dependencies";
    plan.steps.push_back(std::move(installLibsStep));

    return plan;
}

} // namespace dawn::core
