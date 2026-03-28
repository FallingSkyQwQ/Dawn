#include "dawn/core/minecraft/version_package.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;

std::string read_string_field(const Value::Object& object, const std::string& key) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_string()) {
        return value->as_string();
    }
    return {};
}

std::size_t read_size_field(const Value::Object& object, const std::string& key) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_number()) {
        return static_cast<std::size_t>(value->as_number());
    }
    return 0;
}

int read_int_field(const Value::Object& object, const std::string& key) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_number()) {
        return static_cast<int>(value->as_number());
    }
    return 0;
}

bool parse_library_rule(const Value& value, LibraryRule* rule) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    rule->action = read_string_field(object, "action");

    const auto* os = dawn::infra::json::find(object, "os");
    if (os && os->is_object()) {
        const auto& osObject = os->as_object();
        rule->osName = read_string_field(osObject, "name");
        rule->osVersion = read_string_field(osObject, "version");
        rule->osArch = read_string_field(osObject, "arch");
    }

    return true;
}

bool parse_library_download(const Value& value, LibraryDownload* download) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    download->path = read_string_field(object, "path");
    download->url = read_string_field(object, "url");
    download->sha1 = read_string_field(object, "sha1");
    download->size = read_size_field(object, "size");

    return true;
}

bool parse_library_artifact(const Value& value, LibraryArtifact* artifact) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    artifact->path = read_string_field(object, "path");
    artifact->url = read_string_field(object, "url");
    artifact->sha1 = read_string_field(object, "sha1");
    artifact->size = read_size_field(object, "size");

    return true;
}

bool parse_native_downloads(const Value& value, NativeDownloads* natives) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    const auto* classifiers = dawn::infra::json::find(object, "classifiers");
    if (classifiers && classifiers->is_object()) {
        const auto& classObject = classifiers->as_object();

        const auto* linux = dawn::infra::json::find(classObject, "natives-linux");
        if (linux && linux->is_object()) {
            LibraryArtifact artifact;
            if (parse_library_artifact(*linux, &artifact)) {
                natives->linux = std::move(artifact);
            }
        }

        const auto* windows = dawn::infra::json::find(classObject, "natives-windows");
        if (windows && windows->is_object()) {
            LibraryArtifact artifact;
            if (parse_library_artifact(*windows, &artifact)) {
                natives->windows = std::move(artifact);
            }
        }

        const auto* windows64 = dawn::infra::json::find(classObject, "natives-windows-64");
        if (windows64 && windows64->is_object()) {
            LibraryArtifact artifact;
            if (parse_library_artifact(*windows64, &artifact)) {
                natives->windows = std::move(artifact);
            }
        }

        const auto* osx = dawn::infra::json::find(classObject, "natives-osx");
        if (osx && osx->is_object()) {
            LibraryArtifact artifact;
            if (parse_library_artifact(*osx, &artifact)) {
                natives->osx = std::move(artifact);
            }
        }
    }

    return true;
}

bool parse_library(const Value& value, Library* library) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    library->name = read_string_field(object, "name");
    library->url = read_string_field(object, "url");

    // Check if it's a native library
    const auto* downloads = dawn::infra::json::find(object, "downloads");
    if (downloads && downloads->is_object()) {
        const auto& downloadsObject = downloads->as_object();

        const auto* artifact = dawn::infra::json::find(downloadsObject, "artifact");
        if (artifact && artifact->is_object()) {
            LibraryDownload download;
            if (parse_library_download(*artifact, &download)) {
                library->download = std::move(download);
            }
        }

        NativeDownloads natives;
        if (parse_native_downloads(*downloads, &natives)) {
            if (natives.linux || natives.windows || natives.osx) {
                library->natives = std::move(natives);
                library->isNative = true;
            }
        }
    }

    // Parse rules
    const auto* rules = dawn::infra::json::find(object, "rules");
    if (rules && rules->is_array()) {
        for (const auto& ruleValue : rules->as_array()) {
            LibraryRule rule;
            if (parse_library_rule(ruleValue, &rule)) {
                library->rules.push_back(std::move(rule));
            }
        }
    }

    // Parse extract exclusions
    const auto* extract = dawn::infra::json::find(object, "extract");
    if (extract && extract->is_object()) {
        const auto& extractObject = extract->as_object();
        const auto* exclude = dawn::infra::json::find(extractObject, "exclude");
        if (exclude && exclude->is_array()) {
            for (const auto& excludeValue : exclude->as_array()) {
                if (excludeValue.is_string()) {
                    library->extractExclude.push_back(excludeValue.as_string());
                }
            }
        }
    }

    // Check for native classifier in name (e.g., "natives-windows")
    if (library->name.find("natives-") != std::string::npos) {
        library->isNative = true;
    }

    return true;
}

bool parse_asset_index(const Value& value, AssetIndex* assetIndex) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    assetIndex->id = read_string_field(object, "id");
    assetIndex->url = read_string_field(object, "url");
    assetIndex->sha1 = read_string_field(object, "sha1");
    assetIndex->size = read_size_field(object, "size");
    assetIndex->totalSize = read_size_field(object, "totalSize");

    return true;
}

bool parse_download_artifact(const Value& value, DownloadArtifact* artifact) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    artifact->url = read_string_field(object, "url");
    artifact->sha1 = read_string_field(object, "sha1");
    artifact->size = read_size_field(object, "size");

    return true;
}

bool parse_downloads(const Value& value, Downloads* downloads) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    const auto* client = dawn::infra::json::find(object, "client");
    if (client && client->is_object()) {
        DownloadArtifact artifact;
        if (parse_download_artifact(*client, &artifact)) {
            downloads->client = std::move(artifact);
        }
    }

    const auto* server = dawn::infra::json::find(object, "server");
    if (server && server->is_object()) {
        DownloadArtifact artifact;
        if (parse_download_artifact(*server, &artifact)) {
            downloads->server = std::move(artifact);
        }
    }

    const auto* clientMappings = dawn::infra::json::find(object, "client_mappings");
    if (clientMappings && clientMappings->is_object()) {
        DownloadArtifact artifact;
        if (parse_download_artifact(*clientMappings, &artifact)) {
            downloads->client_mappings = std::move(artifact);
        }
    }

    const auto* serverMappings = dawn::infra::json::find(object, "server_mappings");
    if (serverMappings && serverMappings->is_object()) {
        DownloadArtifact artifact;
        if (parse_download_artifact(*serverMappings, &artifact)) {
            downloads->server_mappings = std::move(artifact);
        }
    }

    const auto* windowsServer = dawn::infra::json::find(object, "windows_server");
    if (windowsServer && windowsServer->is_object()) {
        DownloadArtifact artifact;
        if (parse_download_artifact(*windowsServer, &artifact)) {
            downloads->windows_server = std::move(artifact);
        }
    }

    return true;
}

bool parse_java_version(const Value& value, JavaVersion* javaVersion) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    javaVersion->component = read_string_field(object, "component");
    javaVersion->majorVersion = read_int_field(object, "majorVersion");

    return true;
}

bool parse_logging_config(const Value& value, LoggingConfig* logging) {
    if (!value.is_object()) {
        return false;
    }
    const auto& object = value.as_object();

    const auto* client = dawn::infra::json::find(object, "client");
    if (client && client->is_object()) {
        const auto& clientObject = client->as_object();
        logging->argument = read_string_field(clientObject, "argument");

        const auto* file = dawn::infra::json::find(clientObject, "file");
        if (file && file->is_object()) {
            const auto& fileObject = file->as_object();
            LoggingConfig::LoggingFile loggingFile;
            loggingFile.id = read_string_field(fileObject, "id");
            loggingFile.url = read_string_field(fileObject, "url");
            loggingFile.sha1 = read_string_field(fileObject, "sha1");
            loggingFile.size = read_size_field(fileObject, "size");
            logging->file = std::move(loggingFile);
        }
    }

    return true;
}

void parse_arguments(const Value& value, std::vector<std::string>* jvmArgs, std::vector<std::string>* gameArgs) {
    if (!value.is_object()) {
        return;
    }
    const auto& object = value.as_object();

    const auto* jvm = dawn::infra::json::find(object, "jvm");
    if (jvm && jvm->is_array()) {
        for (const auto& argValue : jvm->as_array()) {
            if (argValue.is_string()) {
                jvmArgs->push_back(argValue.as_string());
            } else if (argValue.is_object()) {
                // Complex argument with rules - simplified handling
                const auto& argObject = argValue.as_object();
                const auto* value = dawn::infra::json::find(argObject, "value");
                if (value) {
                    if (value->is_string()) {
                        jvmArgs->push_back(value->as_string());
                    } else if (value->is_array()) {
                        for (const auto& v : value->as_array()) {
                            if (v.is_string()) {
                                jvmArgs->push_back(v.as_string());
                            }
                        }
                    }
                }
            }
        }
    }

    const auto* game = dawn::infra::json::find(object, "game");
    if (game && game->is_array()) {
        for (const auto& argValue : game->as_array()) {
            if (argValue.is_string()) {
                gameArgs->push_back(argValue.as_string());
            } else if (argValue.is_object()) {
                const auto& argObject = argValue.as_object();
                const auto* value = dawn::infra::json::find(argObject, "value");
                if (value) {
                    if (value->is_string()) {
                        gameArgs->push_back(value->as_string());
                    } else if (value->is_array()) {
                        for (const auto& v : value->as_array()) {
                            if (v.is_string()) {
                                gameArgs->push_back(v.as_string());
                            }
                        }
                    }
                }
            }
        }
    }
}

} // namespace

// Library Maven coordinate parsing
std::string Library::group_id() const {
    auto pos = name.find(':');
    if (pos == std::string::npos) {
        return {};
    }
    return name.substr(0, pos);
}

std::string Library::artifact_id() const {
    auto first = name.find(':');
    if (first == std::string::npos) {
        return {};
    }
    auto second = name.find(':', first + 1);
    if (second == std::string::npos) {
        return name.substr(first + 1);
    }
    return name.substr(first + 1, second - first - 1);
}

std::string Library::version() const {
    auto first = name.find(':');
    if (first == std::string::npos) {
        return {};
    }
    auto second = name.find(':', first + 1);
    if (second == std::string::npos) {
        return {};
    }
    auto third = name.find(':', second + 1);
    if (third == std::string::npos) {
        return name.substr(second + 1);
    }
    return name.substr(second + 1, third - second - 1);
}

std::string Library::classifier() const {
    auto first = name.find(':');
    if (first == std::string::npos) {
        return {};
    }
    auto second = name.find(':', first + 1);
    if (second == std::string::npos) {
        return {};
    }
    auto third = name.find(':', second + 1);
    if (third == std::string::npos) {
        return {};
    }
    auto at = name.find('@', third + 1);
    if (at == std::string::npos) {
        return name.substr(third + 1);
    }
    return name.substr(third + 1, at - third - 1);
}

std::string Library::extension() const {
    auto at = name.find('@');
    if (at == std::string::npos) {
        return "jar";
    }
    return name.substr(at + 1);
}

bool parse_version_package(const std::string& json, MinecraftVersionPackage* package, std::string* error) {
    if (!package) {
        if (error) {
            *error = "package pointer is null";
        }
        return false;
    }

    const auto result = dawn::infra::json::parse(json);
    if (!result.ok) {
        if (error) {
            *error = result.error.message;
        }
        return false;
    }

    if (!result.value.is_object()) {
        if (error) {
            *error = "root value is not an object";
        }
        return false;
    }

    const auto& object = result.value.as_object();

    package->versionId = read_string_field(object, "id");
    package->type = read_string_field(object, "type");
    package->releaseTime = read_string_field(object, "releaseTime");
    package->time = read_string_field(object, "time");
    package->inheritsFrom = read_string_field(object, "inheritsFrom");
    package->assets = read_string_field(object, "assets");
    package->mainClass = read_string_field(object, "mainClass");
    package->minimumLauncherVersion = read_int_field(object, "minimumLauncherVersion");
    package->complianceLevel = read_string_field(object, "complianceLevel");

    // Parse minecraftArguments (legacy)
    const auto* minecraftArgs = dawn::infra::json::find(object, "minecraftArguments");
    if (minecraftArgs && minecraftArgs->is_string()) {
        package->minecraftArguments = minecraftArgs->as_string();
    }

    // Parse downloads
    const auto* downloads = dawn::infra::json::find(object, "downloads");
    if (downloads && downloads->is_object()) {
        parse_downloads(*downloads, &package->downloads);
    }

    // Parse libraries
    const auto* libraries = dawn::infra::json::find(object, "libraries");
    if (libraries && libraries->is_array()) {
        for (const auto& libValue : libraries->as_array()) {
            Library library;
            if (parse_library(libValue, &library)) {
                package->libraries.push_back(std::move(library));
            }
        }
    }

    // Parse asset index
    const auto* assetIndex = dawn::infra::json::find(object, "assetIndex");
    if (assetIndex && assetIndex->is_object()) {
        parse_asset_index(*assetIndex, &package->assetIndex);
    }

    // Parse Java version
    const auto* javaVersion = dawn::infra::json::find(object, "javaVersion");
    if (javaVersion && javaVersion->is_object()) {
        JavaVersion jv;
        if (parse_java_version(*javaVersion, &jv)) {
            package->javaVersion = std::move(jv);
        }
    }

    // Parse logging
    const auto* logging = dawn::infra::json::find(object, "logging");
    if (logging && logging->is_object()) {
        LoggingConfig lc;
        if (parse_logging_config(*logging, &lc)) {
            package->logging = std::move(lc);
        }
    }

    // Parse arguments (modern format)
    const auto* arguments = dawn::infra::json::find(object, "arguments");
    if (arguments && arguments->is_object()) {
        parse_arguments(*arguments, &package->jvmArguments, &package->gameArguments);
    }

    return true;
}

bool parse_version_package_file(const std::filesystem::path& path, MinecraftVersionPackage* package, std::string* error) {
    std::string content;
    if (!dawn::infra::fs::read_text_file(path, &content, error)) {
        return false;
    }
    return parse_version_package(content, package, error);
}

std::string get_platform_name() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "osx";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string get_architecture_name() {
#if defined(_WIN64) || defined(__x86_64__) || defined(__amd64__)
    return "x64";
#elif defined(_WIN32) || defined(__i386__)
    return "x86";
#elif defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#else
    return "unknown";
#endif
}

std::optional<std::string> get_native_classifier() {
#if defined(_WIN32)
    return "natives-windows";
#elif defined(__APPLE__)
    return "natives-osx";
#elif defined(__linux__)
    return "natives-linux";
#else
    return std::nullopt;
#endif
}

bool should_use_library(const Library& library) {
    if (library.rules.empty()) {
        return true;
    }

    bool use = false;
    const std::string platform = get_platform_name();
    const std::string arch = get_architecture_name();

    for (const auto& rule : library.rules) {
        bool matches = true;

        if (rule.osName.has_value()) {
            if (rule.osName.value() != platform) {
                matches = false;
            }
        }

        if (matches && rule.osArch.has_value()) {
            if (rule.osArch.value() != arch) {
                matches = false;
            }
        }

        // Note: osVersion matching is simplified (regex not implemented)

        if (matches) {
            use = (rule.action == "allow");
        }
    }

    return use;
}

} // namespace dawn::core
