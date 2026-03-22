#include "dawn/core/diagnostics/diagnostics_service.h"

#include <gtest/gtest.h>

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
