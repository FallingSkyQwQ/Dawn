#pragma once

#include "dawn/core/model/content_types.h"
#include "dawn/core/model/task_types.h"

namespace dawn::core {

class IContentProvider {
public:
    virtual ~IContentProvider() = default;

    virtual SearchResult search(const SearchQuery& query) = 0;
    virtual std::vector<ContentVersion> versions(const std::string& projectId) = 0;
    virtual DependencyGraph resolveDependencies(const InstallRequest& request) = 0;
    virtual TaskPlan buildInstallPlan(const InstallRequest& request) = 0;
};

} // namespace dawn::core
