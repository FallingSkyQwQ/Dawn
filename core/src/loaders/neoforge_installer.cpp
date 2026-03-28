#include "dawn/core/loaders/neoforge_installer.h"

#include "dawn/infra/json/simple_json.h"
#include "dawn/infra/net/http_client_factory.h"

#include <chrono>
#include <format>
#include <regex>
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

std::string to_neoforge_family(const std::string& mcVersion) {
    // NeoForge uses different versioning scheme
    // 1.20.1 -> 47.x, 1.20.2 -> 48.x, 1.20.3 -> 49.x, 1.20.4 -> 50.x
    // 1.21 -> 51.x, etc.
    if (mcVersion.rfind("1.20.1", 0) == 0) {
        return "47";
    } else if (mcVersion.rfind("1.20.2", 0) == 0) {
        return "48";
    } else if (mcVersion.rfind("1.20.3", 0) == 0) {
        return "49";
    } else if (mcVersion.rfind("1.20.4", 0) == 0) {
        return "50";
    } else if (mcVersion.rfind("1.20.5", 0) == 0) {
        return "51";
    } else if (mcVersion.rfind("1.20.6", 0) == 0) {
        return "52";
    } else if (mcVersion.rfind("1.21", 0) == 0) {
        return "21";
    }
    return mcVersion;
}

} // namespace

struct NeoForgeInstaller::Impl {
    static constexpr std::string_view NEOFORGE_MAVEN = "https://maven.neoforged.net/releases";
    static constexpr std::string_view META_URL = "https://maven.neoforged.net/releases/net/neoforged/neoforge/maven-metadata.xml";

    std::vector<LoaderVersion> parse_maven_metadata(const std::string& mcVersion, const std::string& xml) {
        std::vector<LoaderVersion> result;
        const auto family = to_neoforge_family(mcVersion);

        // Parse version tags from XML
        std::regex version_regex(R"(<version>([^<]+)</version>)");
        std::sregex_iterator it(xml.begin(), xml.end(), version_regex);
        std::sregex_iterator end;

        std::vector<std::string> versions;
        for (; it != end; ++it) {
            const auto version = (*it)[1].str();
            // NeoForge versions start with the family number
            if (version.rfind(family + ".", 0) == 0) {
                versions.push_back(version);
            }
        }

        // Sort versions (they should already be sorted from Maven, but let's be safe)
        // Return the most recent versions first
        for (auto it = versions.rbegin(); it != versions.rend() && result.size() < 10; ++it) {
            result.push_back(LoaderVersion{
                .versionId = *it,
                .mcVersion = mcVersion,
                .loaderType = LoaderType::NeoForge
            });
        }

        return result;
    }

    std::string build_installer_url(const std::string& version) {
        std::ostringstream oss;
        oss << NEOFORGE_MAVEN << "/net/neoforged/neoforge/" << version
            << "/neoforge-" << version << "-installer.jar";
        return oss.str();
    }

    std::string build_universal_url(const std::string& version) {
        std::ostringstream oss;
        oss << NEOFORGE_MAVEN << "/net/neoforged/neoforge/" << version
            << "/neoforge-" << version << "-universal.jar";
        return oss.str();
    }
};

NeoForgeInstaller::NeoForgeInstaller() : impl_(std::make_unique<Impl>()) {}

std::vector<LoaderVersion> NeoForgeInstaller::listVersions(const std::string& mcVersion) {
    const auto body = fetch_text(std::string(Impl::META_URL));
    if (body.empty()) {
        return {};
    }

    return impl_->parse_maven_metadata(mcVersion, body);
}

TaskPlan NeoForgeInstaller::buildInstallPlan(const LoaderInstallRequest& request) {
    TaskPlan plan;
    plan.id = std::format("neoforge_install_{}_{}", request.instanceId, request.versionId);
    plan.title = std::format("Install NeoForge {} for {}", request.versionId, request.mcVersion);
    plan.status = TaskStatus::Pending;
    plan.createdAt = get_current_timestamp();
    plan.updatedAt = plan.createdAt;

    // Step 1: Download NeoForge Installer
    TaskStep downloadInstallerStep;
    downloadInstallerStep.id = std::format("{}_download_installer", plan.id);
    downloadInstallerStep.title = "Download NeoForge Installer";
    downloadInstallerStep.status = TaskStatus::Pending;
    downloadInstallerStep.detail = impl_->build_installer_url(request.versionId);
    plan.steps.push_back(std::move(downloadInstallerStep));

    // Step 2: Execute Installer
    TaskStep runInstallerStep;
    runInstallerStep.id = std::format("{}_run_installer", plan.id);
    runInstallerStep.title = "Execute NeoForge Installer";
    runInstallerStep.status = TaskStatus::Pending;
    runInstallerStep.detail = "java -jar neoforge-installer.jar --installServer";
    plan.steps.push_back(std::move(runInstallerStep));

    // Step 3: Install Libraries
    TaskStep installLibsStep;
    installLibsStep.id = std::format("{}_install_libs", plan.id);
    installLibsStep.title = "Install NeoForge Libraries";
    installLibsStep.status = TaskStatus::Pending;
    installLibsStep.detail = "NeoForge dependencies";
    plan.steps.push_back(std::move(installLibsStep));

    // Step 4: Generate Launch Profile
    TaskStep generateProfileStep;
    generateProfileStep.id = std::format("{}_generate_profile", plan.id);
    generateProfileStep.title = "Generate Launch Profile";
    generateProfileStep.status = TaskStatus::Pending;
    generateProfileStep.detail = std::format("instance:{}, java:17+", request.instanceId);
    plan.steps.push_back(std::move(generateProfileStep));

    // Step 5: Setup Version JSON
    TaskStep setupVersionStep;
    setupVersionStep.id = std::format("{}_setup_version", plan.id);
    setupVersionStep.title = "Setup Version Configuration";
    setupVersionStep.status = TaskStatus::Pending;
    setupVersionStep.detail = std::format("neoforge-{}", request.versionId);
    plan.steps.push_back(std::move(setupVersionStep));

    return plan;
}

} // namespace dawn::core
