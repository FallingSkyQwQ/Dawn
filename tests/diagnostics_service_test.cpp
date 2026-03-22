#include "dawn/core/diagnostics/diagnostics_service.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace dawn::core;

TEST(DiagnosticsService, MatchesJavaMismatchAndDependencyRules) {
    const std::string log = R"(java.lang.UnsupportedClassVersionError
Caused by: java.lang.NoClassDefFoundError
OpenGL error)";

    const auto report = DiagnosticsService{}.analyze_log(log);
    EXPECT_TRUE(report.actionable);
    EXPECT_GE(report.findings.size(), 2u);
    EXPECT_EQ(report.findings.front().category, DiagnosticCategory::JavaMismatch);
}

TEST(DiagnosticsService, MatchesConfigCorruptionRule) {
    const std::string log = "config corrupted: json parse error";
    const auto report = DiagnosticsService{}.analyze_log(log);
    ASSERT_FALSE(report.findings.empty());
    EXPECT_EQ(report.findings.back().category, DiagnosticCategory::ConfigCorruption);
}

TEST(DiagnosticsService, BuildsAndExecutesRepairActions) {
    const std::string log = R"(UnsupportedClassVersionError
Missing dependency
config corrupted)";
    DiagnosticsService service;
    const auto report = service.analyze_log(log);
    const auto actions = service.build_repair_actions(report);
    EXPECT_FALSE(actions.empty());

    const auto root = std::filesystem::temp_directory_path() / "dawn-diagnostics-repair";
    std::filesystem::remove_all(root);
    const auto execution = service.execute_repair_actions(actions, root);
    EXPECT_TRUE(execution.success);
    EXPECT_FALSE(execution.logs.empty());
    EXPECT_TRUE(std::filesystem::exists(root / "config" / "dawn" / "repair-actions.log"));
    std::filesystem::remove_all(root);
}
