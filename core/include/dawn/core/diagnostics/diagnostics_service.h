#pragma once

#include "dawn/core/model/preflight.h"

#include <filesystem>
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

struct RepairAction {
    std::string id;
    std::string title;
    std::string detail;
};

struct RepairExecutionResult {
    bool success = false;
    std::vector<std::string> logs;
};

class DiagnosticsService {
public:
    DiagnosticReport analyze_log(const std::string& logText) const;
    std::vector<RepairAction> build_repair_actions(const DiagnosticReport& report) const;
    RepairExecutionResult execute_repair_actions(const std::vector<RepairAction>& actions, const std::filesystem::path& gameDir) const;
};

} // namespace dawn::core
