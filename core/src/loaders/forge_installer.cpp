#include "dawn/core/loaders/forge_installer.h"

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

int parse_mc_version_to_int(const std::string& mcVersion) {
    // Parse version like "1.20.4" to 12004 for comparison
    std::regex version_regex(R"((\d+)\.(\d+)\.?(\d*))");
    std::smatch match;
    if (std::regex_search(mcVersion, match, version_regex)) {
        int major = std::stoi(match[1].str());
        int minor = std::stoi(match[2].str());
        int patch = match[3].str().empty() ? 0 : std::stoi(match[3].str());
        return major * 10000 + minor * 100 + patch;
    }
    return 0;
}

bool requires_java_17(const std::string& mcVersion) {
    // Forge 1.17+ requires Java 17
    return parse_mc_version_to_int(mcVersion) >= 11700;
}

} // namespace

struct ForgeInstaller::Impl {
    static constexpr std::string_view FORGE_MAVEN = "https://maven.minecraftforge.net";
    static constexpr std::string_view FILES_URL = "https://files.minecraftforge.net";

    std::vector<LoaderVersion> parse_promotions(const std::string& mcVersion, const std::string& json) {
        std::vector<LoaderVersion> result;

        const auto parsed = dawn::infra::json::parse(json);
        if (!parsed.ok || !parsed.value.is_object()) {
            return result;
        }

        const auto* promos = dawn::infra::json::find(parsed.value.as_object(), "promos");
        if (!promos || !promos->is_object()) {
            return result;
        }

        // Try recommended version first
        const auto recommended = read_string_field(promos->as_object(), mcVersion + "-recommended");
        if (!recommended.empty()) {
            result.push_back(LoaderVersion{
                .versionId = recommended,
                .mcVersion = mcVersion,
                .loaderType = LoaderType::Forge
            });
        }

        // Then latest version
        const auto latest = read_string_field(promos->as_object(), mcVersion + "-latest");
        if (!latest.empty() && latest != recommended) {
            result.push_back(LoaderVersion{
                .versionId = latest,
                .mcVersion = mcVersion,
                .loaderType = LoaderType::Forge
            });
        }

        return result;
    }

    std::string build_installer_url(const std::string& mcVersion, const std::string& forgeVersion) {
        std::ostringstream oss;
        oss << FORGE_MAVEN << "/net/minecraftforge/forge/" << mcVersion << "-" << forgeVersion
            << "/forge-" << mcVersion << "-" << forgeVersion << "-installer.jar";
        return oss.str();
    }

    std::string build_installer_url_universal(const std::string& mcVersion, const std::string& forgeVersion) {
        std::ostringstream oss;
        oss << FORGE_MAVEN << "/net/minecraftforge/forge/" << mcVersion << "-" << forgeVersion
            << "/forge-" << mcVersion << "-" << forgeVersion << "-universal.jar";
        return oss.str();
    }

    std::string get_java_version_hint(const std::string& mcVersion) {
        return requires_java_17(mcVersion) ? "17" : "8";
    }

    std::vector<std::string> get_installer_arguments(const std::string& mcVersion) {
        std::vector<std::string> args;
        if (requires_java_17(mcVersion)) {
            // Modern Forge (1.17+) uses different installer approach
            args.push_back("--installServer");
        } else {
            // Legacy Forge uses different arguments
            args.push_back("--installClient");
        }
        return args;
    }
};

ForgeInstaller::ForgeInstaller() : impl_(std::make_unique<Impl>()) {}

std::vector<LoaderVersion> ForgeInstaller::listVersions(const std::string& mcVersion) {
    std::string url = std::string(Impl::FILES_URL) + "/net/minecraftforge/forge/promotions_slim.json";

    const auto body = fetch_text(url);
    if (body.empty()) {
        return {};
    }

    return impl_->parse_promotions(mcVersion, body);
}

TaskPlan ForgeInstaller::buildInstallPlan(const LoaderInstallRequest& request) {
    TaskPlan plan;
    plan.id = std::format("forge_install_{}_{}", request.instanceId, request.versionId);
    plan.title = std::format("Install Forge {} for {}", request.versionId, request.mcVersion);
    plan.status = TaskStatus::Pending;
    plan.createdAt = get_current_timestamp();
    plan.updatedAt = plan.createdAt;

    const bool isModern = requires_java_17(request.mcVersion);
    const std::string javaHint = impl_->get_java_version_hint(request.mcVersion);

    // Step 1: Download Forge Installer
    TaskStep downloadInstallerStep;
    downloadInstallerStep.id = std::format("{}_download_installer", plan.id);
    downloadInstallerStep.title = "Download Forge Installer";
    downloadInstallerStep.status = TaskStatus::Pending;
    downloadInstallerStep.detail = impl_->build_installer_url(request.mcVersion, request.versionId);
    plan.steps.push_back(std::move(downloadInstallerStep));

    // Step 2: Run Installer (for modern versions)
    if (isModern) {
        TaskStep runInstallerStep;
        runInstallerStep.id = std::format("{}_run_installer", plan.id);
        runInstallerStep.title = "Execute Forge Installer";
        runInstallerStep.status = TaskStatus::Pending;
        runInstallerStep.detail = std::format("java -jar forge-installer.jar --installServer");
        plan.steps.push_back(std::move(runInstallerStep));
    } else {
        // For legacy versions, download universal jar
        TaskStep downloadUniversalStep;
        downloadUniversalStep.id = std::format("{}_download_universal", plan.id);
        downloadUniversalStep.title = "Download Forge Universal";
        downloadUniversalStep.status = TaskStatus::Pending;
        downloadUniversalStep.detail = impl_->build_installer_url_universal(request.mcVersion, request.versionId);
        plan.steps.push_back(std::move(downloadUniversalStep));

        // Run legacy installer
        TaskStep runInstallerStep;
        runInstallerStep.id = std::format("{}_run_installer", plan.id);
        runInstallerStep.title = "Execute Legacy Installer";
        runInstallerStep.status = TaskStatus::Pending;
        runInstallerStep.detail = std::format("java -jar forge-installer.jar --installClient");
        plan.steps.push_back(std::move(runInstallerStep));
    }

    // Step 3: Extract/Install Libraries
    TaskStep installLibsStep;
    installLibsStep.id = std::format("{}_install_libs", plan.id);
    installLibsStep.title = "Install Forge Libraries";
    installLibsStep.status = TaskStatus::Pending;
    installLibsStep.detail = isModern ? "Modern Forge libraries" : "Legacy Forge libraries";
    plan.steps.push_back(std::move(installLibsStep));

    // Step 4: Generate Launch Profile
    TaskStep generateProfileStep;
    generateProfileStep.id = std::format("{}_generate_profile", plan.id);
    generateProfileStep.title = "Generate Launch Profile";
    generateProfileStep.status = TaskStatus::Pending;
    generateProfileStep.detail = std::format("instance:{}, java:{}", request.instanceId, javaHint);
    plan.steps.push_back(std::move(generateProfileStep));

    // Step 5: Setup Version JSON
    TaskStep setupVersionStep;
    setupVersionStep.id = std::format("{}_setup_version", plan.id);
    setupVersionStep.title = "Setup Version Configuration";
    setupVersionStep.status = TaskStatus::Pending;
    setupVersionStep.detail = std::format("{}-{}", request.mcVersion, request.versionId);
    plan.steps.push_back(std::move(setupVersionStep));

    return plan;
}

} // namespace dawn::core
