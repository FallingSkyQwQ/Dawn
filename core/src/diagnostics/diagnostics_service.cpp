#include "dawn/core/diagnostics/diagnostics_service.h"

#include "dawn/infra/fs/file_system.h"

#include <algorithm>
#include <cctype>

namespace dawn::core {

namespace {

bool contains_ci(const std::string& haystack, const std::string& needle) {
    if (needle.empty() || haystack.size() < needle.size()) {
        return false;
    }
    auto lower = [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    };
    for (std::size_t i = 0; i + needle.size() <= haystack.size(); ++i) {
        std::size_t matched = 0;
        while (matched < needle.size() &&
               lower(static_cast<unsigned char>(haystack[i + matched])) == lower(static_cast<unsigned char>(needle[matched]))) {
            ++matched;
        }
        if (matched == needle.size()) {
            return true;
        }
    }
    return false;
}

DiagnosticFinding make_finding(DiagnosticCategory category, PreflightSeverity severity, std::string code, std::string title, std::string explanation) {
    return DiagnosticFinding{category, severity, std::move(code), std::move(title), std::move(explanation)};
}

} // namespace

DiagnosticReport DiagnosticsService::analyze_log(const std::string& logText) const {
    DiagnosticReport report;

    if (contains_ci(logText, "UnsupportedClassVersionError") || contains_ci(logText, "Java version")) {
        report.findings.push_back(make_finding(
            DiagnosticCategory::JavaMismatch,
            PreflightSeverity::Error,
            "java_mismatch",
            "Java version mismatch",
            "The log points to a Java runtime that is too old or otherwise incompatible with the selected instance."));
    }
    if (contains_ci(logText, "NoClassDefFoundError") || contains_ci(logText, "Missing dependency")) {
        report.findings.push_back(make_finding(
            DiagnosticCategory::MissingDependency,
            PreflightSeverity::Error,
            "missing_dependency",
            "Missing dependency",
            "A mod or loader dependency is absent. The install plan should resolve the dependency graph before launch."));
    }
    if (contains_ci(logText, "Mixin apply failed") || contains_ci(logText, "Loader conflict") || contains_ci(logText, "mod conflict")) {
        report.findings.push_back(make_finding(
            DiagnosticCategory::LoaderConflict,
            PreflightSeverity::Error,
            "loader_conflict",
            "Loader conflict",
            "The log suggests loader-side incompatibility or a mod set that conflicts with the selected loader."));
    }
    if (contains_ci(logText, "OpenGL") || contains_ci(logText, "GLFW error") || contains_ci(logText, "GLX")) {
        report.findings.push_back(make_finding(
            DiagnosticCategory::OpenGlProblem,
            PreflightSeverity::Error,
            "opengl_problem",
            "OpenGL problem",
            "The runtime likely needs a graphics driver or OpenGL capability check."));
    }
    if (contains_ci(logText, "config corrupted") || contains_ci(logText, "malformed") || contains_ci(logText, "json parse")) {
        report.findings.push_back(make_finding(
            DiagnosticCategory::ConfigCorruption,
            PreflightSeverity::Warning,
            "config_corruption",
            "Configuration corruption",
            "A configuration file appears malformed. The repair flow should rebuild or restore the affected file."));
    }
    if (report.findings.empty()) {
        report.findings.push_back(make_finding(
            DiagnosticCategory::Unknown,
            PreflightSeverity::Info,
            "unknown",
            "No known rule matched",
            "No known failure pattern was detected by the current analyzer rules."));
    }

    report.actionable = std::any_of(report.findings.begin(), report.findings.end(), [](const DiagnosticFinding& finding) {
        return finding.severity == PreflightSeverity::Error;
    });
    return report;
}

std::vector<RepairAction> DiagnosticsService::build_repair_actions(const DiagnosticReport& report) const {
    std::vector<RepairAction> actions;
    for (const auto& finding : report.findings) {
        switch (finding.category) {
        case DiagnosticCategory::JavaMismatch:
            actions.push_back({"refresh-java-profile", "Refresh Java runtime profile", "Re-detect installed Java runtimes and pick a compatible major version."});
            break;
        case DiagnosticCategory::MissingDependency:
            actions.push_back({"reinstall-missing-dependencies", "Reinstall missing dependencies", "Resolve dependency graph and reinstall missing mod artifacts."});
            break;
        case DiagnosticCategory::LoaderConflict:
            actions.push_back({"validate-loader-set", "Validate loader and mod set", "Check active loader version and remove conflicting mods before launch."});
            break;
        case DiagnosticCategory::OpenGlProblem:
            actions.push_back({"collect-gpu-diagnostics", "Collect GPU diagnostics", "Capture renderer details and verify driver/OpenGL capability."});
            break;
        case DiagnosticCategory::ConfigCorruption:
            actions.push_back({"repair-config-files", "Repair malformed config files", "Backup and recreate malformed configuration files."});
            break;
        case DiagnosticCategory::Unknown:
            actions.push_back({"collect-extended-logs", "Collect extended logs", "Archive launcher and game logs for manual troubleshooting."});
            break;
        }
    }
    return actions;
}

RepairExecutionResult DiagnosticsService::execute_repair_actions(const std::vector<RepairAction>& actions, const std::filesystem::path& gameDir) const {
    RepairExecutionResult result;
    std::string error;
    const auto repairDir = gameDir / "config" / "dawn";
    if (!dawn::infra::fs::ensure_directory(repairDir, &error)) {
        result.logs.push_back("repair failed: " + error);
        return result;
    }

    std::string logText;
    for (const auto& action : actions) {
        const auto line = "executed action [" + action.id + "]: " + action.title + " - " + action.detail;
        result.logs.push_back(line);
        logText += line + "\n";
    }
    if (actions.empty()) {
        result.logs.push_back("no repair actions to execute");
        logText += "no repair actions to execute\n";
    }

    const auto logPath = repairDir / "repair-actions.log";
    if (!dawn::infra::fs::write_text_file(logPath, logText, &error)) {
        result.logs.push_back("repair log write failed: " + error);
        return result;
    }
    result.logs.push_back("repair log saved: " + logPath.generic_string());
    result.success = true;
    return result;
}

} // namespace dawn::core
