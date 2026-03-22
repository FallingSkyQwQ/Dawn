#pragma once

#include "dawn/core/model/enums.h"

#include <string>
#include <vector>

namespace dawn::core {

struct PreflightIssue {
    PreflightSeverity severity = PreflightSeverity::Info;
    std::string code;
    std::string message;
    std::string suggestion;
};

struct PreflightResult {
    bool ready = false;
    std::vector<PreflightIssue> issues;
    std::vector<std::string> recommendations;
};

} // namespace dawn::core
