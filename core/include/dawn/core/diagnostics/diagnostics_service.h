#pragma once

#include "dawn/core/model/preflight.h"

#include <string>
#include <vector>

namespace dawn::core {

enum class DiagnosticCategory {
    JavaMismatch,
    MissingDependency,
    LoaderConflict,
    OpenGlProblem,
    ConfigCorruption,
    Unknown,
};

struct DiagnosticFinding {
    DiagnosticCategory category = DiagnosticCategory::Unknown;
    PreflightSeverity severity = PreflightSeverity::Info;
    std::string code;
    std::string title;
    std::string explanation;
};

struct DiagnosticReport {
    bool actionable = false;
    std::vector<DiagnosticFinding> findings;
};

class DiagnosticsService {
public:
    DiagnosticReport analyze_log(const std::string& logText) const;
};

} // namespace dawn::core
