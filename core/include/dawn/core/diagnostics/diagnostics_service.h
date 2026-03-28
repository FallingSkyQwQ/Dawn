#pragma once

#include "dawn/core/model/preflight.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace dawn::core {

// 原有的枚举和结构保持兼容
enum class DiagnosticCategory {
    JavaMismatch,
    MissingDependency,
    VersionConflict,
    LoaderConflict,
    OpenGlProblem,
    ConfigCorruption,
    OutOfMemory,
    MissingLibrary,
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

// ========== 新增数据结构 ==========

// 修复步骤类型
enum class FixStepType {
    DownloadLibrary,      // 下载库文件
    DownloadMod,          // 下载模组
    RemoveMod,            // 移除模组
    RepairConfig,         // 修复配置
    UpdateJava,           // 更新Java
    CleanCache,           // 清理缓存
    RestartRequired,      // 需要重启
};

// 修复步骤
struct FixStep {
    FixStepType type;
    std::string description;
    std::string targetId;
    std::string downloadUrl;  // 用于下载类型
    std::filesystem::path targetPath;
    bool requiresConfirmation = false;
};

// 修复计划
struct FixPlan {
    std::vector<FixStep> steps;
    std::string estimatedTime;
    bool requiresRestart = false;
    std::string summary;
};

// 检测到的问题
struct DetectedIssue {
    std::string code;
    std::string severity;  // "error", "warning", "info"
    std::string category;
    std::string technicalDescription;
    std::string userFriendlyDescription;
    std::string suggestedFix;
    bool autoFixable = false;
    std::vector<std::string> affectedFiles;
};

// 日志分析结果
struct LogAnalysisResult {
    bool isCrash = false;
    std::string crashType;
    std::vector<DetectedIssue> issues;
    std::string summary;
    std::string originalLog;
};

// 实例健康检查结果
struct InstanceHealthCheck {
    std::string instanceId;
    bool healthy = true;
    std::vector<DetectedIssue> issues;
    std::string javaStatus;
    std::string dependencyStatus;
    std::string configStatus;
    std::string resourceStatus;
    uint64_t freeDiskSpace = 0;
};

// 修复进度回调
using FixProgressCallback = std::function<void(const std::string& step, int progressPercent)>;

// ========== 诊断服务类 ==========

class DiagnosticsService {
public:
    // 原有的日志分析方法（保持兼容）
    DiagnosticReport analyze_log(const std::string& logText) const;
    std::vector<RepairAction> build_repair_actions(const DiagnosticReport& report) const;
    RepairExecutionResult execute_repair_actions(const std::vector<RepairAction>& actions, const std::filesystem::path& gameDir) const;

    // ========== 新增功能 ==========

    // 1. 日志解释功能
    LogAnalysisResult analyze_launch_log(const std::string& logContent) const;
    LogAnalysisResult analyze_crash_report(const std::string& reportContent) const;

    // 2. 人话总结
    std::string explain_issue(const std::string& issueCode) const;
    DetectedIssue get_issue_details(const std::string& issueCode) const;

    // 3. 常见问题检测
    InstanceHealthCheck detect_common_issues(const std::string& instanceId, const std::filesystem::path& gameDir) const;

    // 4. 一键修复
    FixPlan generate_fix_plan(const std::vector<DetectedIssue>& issues) const;
    FixPlan generate_fix_plan(const LogAnalysisResult& analysis) const;
    bool apply_fix(const FixPlan& fixPlan, const std::filesystem::path& gameDir, 
                   std::vector<std::string>* logs = nullptr,
                   FixProgressCallback progressCallback = nullptr) const;

    // 5. 辅助功能
    std::vector<std::string> get_supported_issue_codes() const;
    bool can_auto_fix(const std::string& issueCode) const;

private:
    // 内部辅助方法
    std::vector<DetectedIssue> detect_java_issues(const std::string& logContent) const;
    std::vector<DetectedIssue> detect_memory_issues(const std::string& logContent) const;
    std::vector<DetectedIssue> detect_dependency_issues(const std::string& logContent) const;
    std::vector<DetectedIssue> detect_loader_issues(const std::string& logContent) const;
    std::vector<DetectedIssue> detect_config_issues(const std::string& logContent) const;
    std::vector<DetectedIssue> detect_graphics_issues(const std::string& logContent) const;
    std::vector<DetectedIssue> detect_library_issues(const std::string& logContent) const;

    // 修复执行辅助
    bool execute_download_step(const FixStep& step, std::vector<std::string>* logs) const;
    bool execute_remove_step(const FixStep& step, std::vector<std::string>* logs) const;
    bool execute_repair_config_step(const FixStep& step, const std::filesystem::path& gameDir, std::vector<std::string>* logs) const;
    bool execute_clean_cache_step(const std::filesystem::path& gameDir, std::vector<std::string>* logs) const;
};

} // namespace dawn::core
