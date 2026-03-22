#pragma once

#include "dawn/core/model/content_lock.h"
#include "dawn/core/model/enums.h"

#include <cstddef>
#include <string>
#include <vector>

namespace dawn::core {

struct SearchQuery {
    std::string text;
    ProjectType projectType = ProjectType::Mod;
    std::vector<std::string> categories;
    std::vector<std::string> gameVersions;
    std::vector<LoaderType> loaders;
    bool clientSide = true;
    bool serverSide = false;
};

struct SearchResultItem {
    std::string projectId;
    std::string title;
    std::string summary;
    std::string author;
    std::string iconUrl;
    std::string updatedAt;
    std::size_t downloads = 0;
    ProjectType projectType = ProjectType::Mod;
    std::vector<std::string> supportedGameVersions;
    std::vector<LoaderType> supportedLoaders;
};

struct SearchResult {
    std::vector<SearchResultItem> items;
    std::string nextCursor;
};

enum class DependencyRequirement {
    Required,
    Optional,
    Incompatible,
    Embedded,
};

struct ContentDependency {
    std::string projectId;
    std::string versionId;
    std::string fileName;
    DependencyRequirement requirement = DependencyRequirement::Required;
    std::string note;
};

struct ContentVersion {
    std::string versionId;
    std::string name;
    std::vector<std::string> fileUrls;
    std::vector<ContentDependency> dependencies;
    std::vector<std::string> gameVersions;
    std::vector<LoaderType> loaders;
};

struct DependencyGraph {
    std::vector<ContentDependency> dependencies;
    std::vector<ContentLock> locks;
    std::vector<std::string> missing;
    std::vector<std::string> optional;
    std::vector<std::string> conflicts;
};

struct InstallDiagnostic {
    std::string code;
    PreflightSeverity severity = PreflightSeverity::Info;
    std::string message;
    std::string suggestion;
    bool blocker = false;
};

struct DependencyCheckResult {
    bool blocked = false;
    DependencyGraph graph;
    std::vector<InstallDiagnostic> diagnostics;
};

struct InstallRequest {
    std::string provider = "modrinth";
    std::string instanceId;
    std::string projectId;
    std::string versionId;
    ProjectType projectType = ProjectType::Mod;
};

struct LoaderVersion {
    std::string versionId;
    std::string mcVersion;
    LoaderType loaderType = LoaderType::None;
};

struct LoaderInstallRequest {
    std::string instanceId;
    std::string mcVersion;
    LoaderType loaderType = LoaderType::None;
    std::string versionId;
};

} // namespace dawn::core
