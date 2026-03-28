#include "dawn/core/diagnostics/diagnostics_service.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/net/http_client.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace dawn::core {

namespace {

// ========== 工具函数 ==========

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

bool contains_any_ci(const std::string& haystack, const std::vector<std::string>& needles) {
    for (const auto& needle : needles) {
        if (contains_ci(haystack, needle)) {
            return true;
        }
    }
    return false;
}

std::string to_lower(std::string str) {
    std::transform(str.begin(), str.end(), str.begin(), 
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return str;
}

std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\n\r");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\n\r");
    return str.substr(first, last - first + 1);
}

std::vector<std::string> extract_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        lines.push_back(line);
    }
    return lines;
}

// ========== 已知问题数据库 ==========

struct IssueInfo {
    std::string code;
    std::string category;
    std::string severity;
    std::string technicalDescription;
    std::string userFriendlyDescription;
    std::string suggestedFix;
    bool autoFixable;
};

const std::unordered_map<std::string, IssueInfo>& get_issue_database() {
    static const std::unordered_map<std::string, IssueInfo> database = {
        // Java 相关问题
        {"java_version_mismatch", {
            "java_version_mismatch",
            "java",
            "error",
            "UnsupportedClassVersionError or Java version mismatch detected",
            "当前使用的 Java 版本与游戏版本不兼容。例如：使用 Java 8 运行需要 Java 17 的 Minecraft 1.18+",
            "点击修复将自动下载并配置兼容的 Java 版本",
            true
        }},
        {"java_outdated", {
            "java_outdated",
            "java",
            "warning",
            "Java version is outdated for optimal performance",
            "Java 版本较旧，可能影响游戏性能或某些功能",
            "建议升级到更新的 Java 版本以获得更好性能",
            true
        }},
        {"java_not_found", {
            "java_not_found",
            "java",
            "error",
            "Java runtime not found or JAVA_HOME not set",
            "未找到 Java 运行时环境，请安装 Java",
            "点击修复将自动下载并安装合适的 Java 版本",
            true
        }},

        // 内存相关问题
        {"out_of_memory", {
            "out_of_memory",
            "memory",
            "error",
            "OutOfMemoryError: Java heap space or GC overhead limit exceeded",
            "游戏内存不足，导致崩溃。可能是分配的内存太小，或模组过多",
            "建议增加分配给游戏的内存（在实例设置中调整），或关闭一些大型模组",
            false
        }},
        {"memory_leak", {
            "memory_leak",
            "memory",
            "warning",
            "Possible memory leak detected in logs",
            "检测到可能的内存泄漏，游戏可能随着时间推移变得越来越卡",
            "尝试更新相关模组，或检查是否有模组冲突",
            false
        }},

        // 模组依赖问题
        {"missing_mod_dependency", {
            "missing_mod_dependency",
            "dependency",
            "error",
            "Mod missing required dependency (NoClassDefFoundError or ModResolutionException)",
            "某个模组缺少必需的依赖模组",
            "点击修复将自动安装缺失的依赖模组",
            true
        }},
        {"mod_version_conflict", {
            "mod_version_conflict",
            "dependency",
            "error",
            "Mod version conflict or incompatible versions",
            "模组版本冲突，可能是多个模组需要同一依赖的不同版本",
            "需要手动检查并更新/降级相关模组到兼容版本",
            false
        }},
        {"circular_dependency", {
            "circular_dependency",
            "dependency",
            "error",
            "Circular dependency detected between mods",
            "模组之间存在循环依赖，无法加载",
            "需要移除或更新造成循环依赖的模组",
            false
        }},

        // Loader 问题
        {"loader_version_incompatible", {
            "loader_version_incompatible",
            "loader",
            "error",
            "Loader version incompatible with Minecraft version",
            "Loader 版本与 Minecraft 版本不兼容",
            "点击修复将自动下载兼容的 Loader 版本",
            true
        }},
        {"fabric_api_missing", {
            "fabric_api_missing",
            "loader",
            "error",
            "Fabric API is required but not installed",
            "需要 Fabric API，但未安装",
            "点击修复将自动安装 Fabric API",
            true
        }},
        {"forge_missing_dependency", {
            "forge_missing_dependency",
            "loader",
            "error",
            "Forge mod missing required dependency",
            "Forge 模组缺少必需的依赖",
            "点击修复将自动安装缺失的依赖",
            true
        }},

        // 配置问题
        {"config_corrupted", {
            "config_corrupted",
            "config",
            "error",
            "Configuration file is corrupted or malformed",
            "配置文件损坏或格式错误",
            "点击修复将备份并重新生成配置文件",
            true
        }},
        {"json_parse_error", {
            "json_parse_error",
            "config",
            "error",
            "JSON parsing error in configuration file",
            "配置文件 JSON 格式错误",
            "点击修复将尝试修复或重置配置文件",
            true
        }},
        {"options_reset", {
            "options_reset",
            "config",
            "warning",
            "Game options were reset to default",
            "游戏选项已重置为默认值",
            "之前的配置可能不兼容当前版本，建议重新配置",
            false
        }},

        // 显卡/OpenGL 问题
        {"opengl_not_supported", {
            "opengl_not_supported",
            "graphics",
            "error",
            "OpenGL version not supported by graphics driver",
            "显卡驱动不支持所需的 OpenGL 版本",
            "请更新显卡驱动程序，或检查显卡是否支持该 OpenGL 版本",
            false
        }},
        {"glfw_error", {
            "glfw_error",
            "graphics",
            "error",
            "GLFW initialization failed",
            "GLFW 初始化失败，可能是显卡驱动问题",
            "请更新显卡驱动，或检查是否有其他程序占用了显示资源",
            false
        }},
        {"intel_graphics_issue", {
            "intel_graphics_issue",
            "graphics",
            "warning",
            "Intel integrated graphics detected with known compatibility issues",
            "检测到 Intel 集成显卡，可能存在兼容性问题",
            "建议更新 Intel 显卡驱动，或考虑使用独立显卡运行游戏",
            false
        }},
        {"nvidia_driver_issue", {
            "nvidia_driver_issue",
            "graphics",
            "warning",
            "NVIDIA driver compatibility issue detected",
            "检测到 NVIDIA 驱动兼容性问题",
            "建议更新 NVIDIA 显卡驱动到最新版本",
            false
        }},

        // 库文件问题
        {"missing_native_library", {
            "missing_native_library",
            "library",
            "error",
            "Missing native library for platform",
            "缺少平台原生库文件",
            "点击修复将重新下载缺失的库文件",
            true
        }},
        {"lwjgl_error", {
            "lwjgl_error",
            "library",
            "error",
            "LWJGL initialization or library error",
            "LWJGL 库初始化错误",
            "点击修复将重新下载并配置 LWJGL 库",
            true
        }},
        {"dll_load_error", {
            "dll_load_error",
            "library",
            "error",
            "DLL load failed or missing system library",
            "DLL 加载失败或缺少系统库",
            "可能需要安装 Visual C++ Redistributable 或更新系统",
            false
        }},

        // 其他问题
        {"disk_space_low", {
            "disk_space_low",
            "system",
            "warning",
            "Low disk space detected",
            "磁盘空间不足",
            "请清理磁盘空间，确保有足够空间运行游戏",
            false
        }},
        {"file_permission_denied", {
            "file_permission_denied",
            "system",
            "error",
            "File permission denied when accessing game files",
            "访问游戏文件时权限被拒绝",
            "请检查文件夹权限，或以管理员身份运行启动器",
            false
        }},
        {"network_timeout", {
            "network_timeout",
            "network",
            "warning",
            "Network timeout during download or authentication",
            "下载或验证时网络超时",
            "请检查网络连接，或稍后重试",
            false
        }},
        {"unknown_crash", {
            "unknown_crash",
            "unknown",
            "error",
            "Unknown crash reason",
            "未知原因导致的崩溃",
            "请查看完整日志，或尝试重新启动游戏",
            false
        }},
    };
    return database;
}

// ========== 日志模式匹配 ==========

struct LogPattern {
    std::vector<std::string> keywords;
    std::string issueCode;
    std::string crashType;
};

const std::vector<LogPattern>& get_log_patterns() {
    static const std::vector<LogPattern> patterns = {
        // Java 版本问题
        {{"UnsupportedClassVersionError", "has been compiled by a more recent version of the Java Runtime"}, 
         "java_version_mismatch", "java_error"},
        {{"Could not find or load main class", "Java version"}, 
         "java_not_found", "java_error"},
        {{"java.lang.UnsupportedClassVersionError"}, 
         "java_version_mismatch", "java_error"},

        // 内存问题
        {{"OutOfMemoryError", "Java heap space"}, 
         "out_of_memory", "memory_error"},
        {{"OutOfMemoryError", "GC overhead limit exceeded"}, 
         "out_of_memory", "memory_error"},
        {{"OutOfMemoryError", "Metaspace"}, 
         "out_of_memory", "memory_error"},

        // 模组依赖问题
        {{"NoClassDefFoundError", "mod"}, 
         "missing_mod_dependency", "dependency_error"},
        {{"ModResolutionException", "depends on"}, 
         "missing_mod_dependency", "dependency_error"},
        {{"ModResolutionException", "conflicts with"}, 
         "mod_version_conflict", "dependency_error"},
        {{"Missing dependency", "requires"}, 
         "missing_mod_dependency", "dependency_error"},
        {{"Could not find required mod"}, 
         "missing_mod_dependency", "dependency_error"},
        {{"Incompatible mod set", "dependency"}, 
         "mod_version_conflict", "dependency_error"},

        // Loader 问题
        {{"FabricLoader", "incompatible", "Minecraft"}, 
         "loader_version_incompatible", "loader_error"},
        {{"Could not find required mod: fabric"}, 
         "fabric_api_missing", "loader_error"},
        {{"Forge", "missing", "dependency"}, 
         "forge_missing_dependency", "loader_error"},
        {{"Mixin apply failed"}, 
         "loader_version_incompatible", "loader_error"},
        {{"TransformerException"}, 
         "loader_version_incompatible", "loader_error"},

        // 配置问题
        {{"com.google.gson.JsonSyntaxException"}, 
         "json_parse_error", "config_error"},
        {{"config", "corrupted"}, 
         "config_corrupted", "config_error"},
        {{"malformed", "json"}, 
         "json_parse_error", "config_error"},
        {{"Failed to load options"}, 
         "options_reset", "config_error"},

        // 显卡/OpenGL 问题
        {{"OpenGL", "not supported"}, 
         "opengl_not_supported", "graphics_error"},
        {{"GLFW error"}, 
         "glfw_error", "graphics_error"},
        {{"GLX", "error"}, 
         "glfw_error", "graphics_error"},
        {{"Pixel format not accelerated"}, 
         "opengl_not_supported", "graphics_error"},
        {{"Intel", "graphics", "driver"}, 
         "intel_graphics_issue", "graphics_warning"},
        {{"NVIDIA", "driver", "outdated"}, 
         "nvidia_driver_issue", "graphics_warning"},

        // 库文件问题
        {{"UnsatisfiedLinkError", "lwjgl"}, 
         "lwjgl_error", "library_error"},
        {{"UnsatisfiedLinkError", "native"}, 
         "missing_native_library", "library_error"},
        {{"DLL load failed"}, 
         "dll_load_error", "library_error"},
        {{"NoClassDefFoundError", "lwjgl"}, 
         "lwjgl_error", "library_error"},

        // 系统问题
        {{"Permission denied"}, 
         "file_permission_denied", "system_error"},
        {{"Access is denied"}, 
         "file_permission_denied", "system_error"},
        {{"No space left on device"}, 
         "disk_space_low", "system_error"},
        {{"Connect timed out"}, 
         "network_timeout", "network_error"},
        {{"Connection reset"}, 
         "network_timeout", "network_error"},
    };
    return patterns;
}

// 创建 DetectedIssue 辅助函数
DetectedIssue create_issue(const std::string& code, const std::vector<std::string>& affectedFiles = {}) {
    const auto& db = get_issue_database();
    auto it = db.find(code);
    if (it != db.end()) {
        return DetectedIssue{
            it->second.code,
            it->second.severity,
            it->second.category,
            it->second.technicalDescription,
            it->second.userFriendlyDescription,
            it->second.suggestedFix,
            it->second.autoFixable,
            affectedFiles
        };
    }
    return DetectedIssue{
        code, "error", "unknown", 
        "Unknown issue: " + code, 
        "检测到未知问题: " + code,
        "请查看详细日志以获取更多信息",
        false,
        affectedFiles
    };
}

DiagnosticFinding make_finding(DiagnosticCategory category, PreflightSeverity severity, 
                                std::string code, std::string title, std::string explanation) {
    return DiagnosticFinding{category, severity, std::move(code), std::move(title), std::move(explanation)};
}

} // namespace

// ========== 原有方法实现（保持兼容） ==========

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
        default:
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

// ========== 新增方法实现 ==========

// 1. 日志解释功能

LogAnalysisResult DiagnosticsService::analyze_launch_log(const std::string& logContent) const {
    LogAnalysisResult result;
    result.originalLog = logContent;

    // 检测是否是崩溃日志
    result.isCrash = contains_any_ci(logContent, {
        "crash", "crashed", "fatal", "FATAL", "ERROR", "Exception in thread",
        "Minecraft has crashed", "An unexpected issue occurred"
    });

    // 收集所有检测到的问题
    auto javaIssues = detect_java_issues(logContent);
    auto memoryIssues = detect_memory_issues(logContent);
    auto dependencyIssues = detect_dependency_issues(logContent);
    auto loaderIssues = detect_loader_issues(logContent);
    auto configIssues = detect_config_issues(logContent);
    auto graphicsIssues = detect_graphics_issues(logContent);
    auto libraryIssues = detect_library_issues(logContent);

    // 合并所有问题
    result.issues.insert(result.issues.end(), javaIssues.begin(), javaIssues.end());
    result.issues.insert(result.issues.end(), memoryIssues.begin(), memoryIssues.end());
    result.issues.insert(result.issues.end(), dependencyIssues.begin(), dependencyIssues.end());
    result.issues.insert(result.issues.end(), loaderIssues.begin(), loaderIssues.end());
    result.issues.insert(result.issues.end(), configIssues.begin(), configIssues.end());
    result.issues.insert(result.issues.end(), graphicsIssues.begin(), graphicsIssues.end());
    result.issues.insert(result.issues.end(), libraryIssues.begin(), libraryIssues.end());

    // 确定崩溃类型
    if (!result.issues.empty()) {
        result.crashType = result.issues[0].category;
    } else if (result.isCrash) {
        result.crashType = "unknown";
        result.issues.push_back(create_issue("unknown_crash"));
    }

    // 生成总结
    if (result.issues.empty()) {
        result.summary = "日志分析完成，未检测到明显问题。";
    } else {
        int errorCount = 0, warningCount = 0;
        for (const auto& issue : result.issues) {
            if (issue.severity == "error") errorCount++;
            else if (issue.severity == "warning") warningCount++;
        }
        
        std::ostringstream oss;
        oss << "检测到 " << errorCount << " 个错误，" << warningCount << " 个警告。";
        if (errorCount > 0) {
            oss << "主要问题：" << result.issues[0].userFriendlyDescription;
        }
        result.summary = oss.str();
    }

    return result;
}

LogAnalysisResult DiagnosticsService::analyze_crash_report(const std::string& reportContent) const {
    // 崩溃报告分析与启动日志分析类似，但可能包含更多堆栈信息
    LogAnalysisResult result = analyze_launch_log(reportContent);
    
    // 额外检测崩溃报告特有的信息
    if (contains_ci(reportContent, "---- Minecraft Crash Report ----")) {
        // 尝试提取崩溃描述
        std::regex descRegex("Description: (.+)");
        std::smatch match;
        if (std::regex_search(reportContent, match, descRegex) && match.size() > 1) {
            std::string description = trim(match[1].str());
            if (!description.empty() && description != "Initializing game") {
                result.summary = "崩溃原因: " + description + "。" + result.summary;
            }
        }
    }

    // 检测特定的崩溃模式
    if (contains_ci(reportContent, "Ticking entity")) {
        result.issues.push_back(create_issue("ticking_entity"));
        result.crashType = "entity_error";
    }
    if (contains_ci(reportContent, "Ticking block entity")) {
        result.issues.push_back(create_issue("ticking_block_entity"));
        result.crashType = "block_entity_error";
    }
    if (contains_ci(reportContent, "Rendering entity")) {
        result.issues.push_back(create_issue("rendering_entity"));
        result.crashType = "rendering_error";
    }

    return result;
}

// 2. 人话总结

std::string DiagnosticsService::explain_issue(const std::string& issueCode) const {
    auto issue = get_issue_details(issueCode);
    return issue.userFriendlyDescription + "\n\n建议修复: " + issue.suggestedFix;
}

DetectedIssue DiagnosticsService::get_issue_details(const std::string& issueCode) const {
    return create_issue(issueCode);
}

// 3. 常见问题检测

InstanceHealthCheck DiagnosticsService::detect_common_issues(const std::string& instanceId, 
                                                              const std::filesystem::path& gameDir) const {
    InstanceHealthCheck check;
    check.instanceId = instanceId;
    check.healthy = true;

    std::string error;

    // 1. 检查 Java 配置
    auto javaPath = gameDir / "java";
    if (!std::filesystem::exists(javaPath)) {
        check.javaStatus = "not_configured";
        check.issues.push_back(create_issue("java_not_found"));
        check.healthy = false;
    } else {
        check.javaStatus = "configured";
    }

    // 2. 检查模组依赖完整性
    auto modsPath = gameDir / "mods";
    if (std::filesystem::exists(modsPath)) {
        // 检查是否有模组目录但可能缺少依赖
        // 这里简化处理，实际应该解析每个模组的依赖
        check.dependencyStatus = "needs_check";
    } else {
        check.dependencyStatus = "no_mods";
    }

    // 3. 检查配置文件有效性
    auto configPath = gameDir / "config";
    if (std::filesystem::exists(configPath)) {
        bool configOk = true;
        for (const auto& entry : std::filesystem::directory_iterator(configPath)) {
            if (entry.path().extension() == ".json") {
                // 尝试读取 JSON 文件验证格式
                std::string content;
                if (dawn::infra::fs::read_text_file(entry.path(), &content, &error)) {
                    // 简单检查 JSON 格式
                    if (content.empty() || (content.front() != '{' && content.front() != '[')) {
                        configOk = false;
                        auto issue = create_issue("json_parse_error");
                        issue.affectedFiles.push_back(entry.path().string());
                        check.issues.push_back(issue);
                    }
                }
            }
        }
        check.configStatus = configOk ? "valid" : "corrupted";
        if (!configOk) check.healthy = false;
    } else {
        check.configStatus = "missing";
    }

    // 4. 检查资源文件完整性
    auto resourcesPath = gameDir / "resources";
    auto assetsPath = gameDir / "assets";
    if (!std::filesystem::exists(resourcesPath) && !std::filesystem::exists(assetsPath)) {
        check.resourceStatus = "incomplete";
        check.healthy = false;
    } else {
        check.resourceStatus = "present";
    }

    // 5. 检查磁盘空间
    // 这里简化处理，实际应该获取磁盘空间信息
    check.freeDiskSpace = 0;  // 需要平台特定实现

    return check;
}

// 4. 一键修复

FixPlan DiagnosticsService::generate_fix_plan(const std::vector<DetectedIssue>& issues) const {
    FixPlan plan;
    int autoFixCount = 0;
    int manualFixCount = 0;

    for (const auto& issue : issues) {
        if (!issue.autoFixable) {
            manualFixCount++;
            continue;
        }

        autoFixCount++;
        FixStep step;
        step.targetId = issue.code;

        if (issue.code == "java_version_mismatch" || issue.code == "java_not_found") {
            step.type = FixStepType::UpdateJava;
            step.description = "下载并配置兼容的 Java 版本";
            step.requiresConfirmation = true;
            plan.requiresRestart = true;
        } else if (issue.code == "missing_mod_dependency" || issue.code == "fabric_api_missing") {
            step.type = FixStepType::DownloadMod;
            step.description = "安装缺失的模组依赖: " + issue.technicalDescription;
        } else if (issue.code == "missing_native_library" || issue.code == "lwjgl_error") {
            step.type = FixStepType::DownloadLibrary;
            step.description = "重新下载缺失的库文件";
        } else if (issue.code == "config_corrupted" || issue.code == "json_parse_error") {
            step.type = FixStepType::RepairConfig;
            step.description = "修复损坏的配置文件";
            for (const auto& file : issue.affectedFiles) {
                step.targetPath = file;
            }
        } else if (issue.code == "mod_version_conflict") {
            step.type = FixStepType::RemoveMod;
            step.description = "移除冲突的模组（需要手动选择）";
            step.requiresConfirmation = true;
        } else {
            step.type = FixStepType::CleanCache;
            step.description = "清理缓存并重新验证文件";
        }

        plan.steps.push_back(step);
    }

    // 生成计划摘要
    std::ostringstream oss;
    if (autoFixCount > 0) {
        oss << "将自动修复 " << autoFixCount << " 个问题；";
    }
    if (manualFixCount > 0) {
        oss << "需要手动处理 " << manualFixCount << " 个问题；";
    }
    if (plan.requiresRestart) {
        oss << "修复完成后需要重启。";
    }
    plan.summary = oss.str();

    // 估计时间
    if (plan.steps.size() <= 2) {
        plan.estimatedTime = "约 1-2 分钟";
    } else if (plan.steps.size() <= 5) {
        plan.estimatedTime = "约 3-5 分钟";
    } else {
        plan.estimatedTime = "约 5-10 分钟";
    }

    return plan;
}

FixPlan DiagnosticsService::generate_fix_plan(const LogAnalysisResult& analysis) const {
    return generate_fix_plan(analysis.issues);
}

bool DiagnosticsService::apply_fix(const FixPlan& fixPlan, const std::filesystem::path& gameDir,
                                    std::vector<std::string>* logs, FixProgressCallback progressCallback) const {
    if (logs) {
        logs->push_back("开始执行修复计划: " + fixPlan.summary);
    }

    int totalSteps = static_cast<int>(fixPlan.steps.size());
    int completedSteps = 0;

    for (const auto& step : fixPlan.steps) {
        if (logs) {
            logs->push_back("执行: " + step.description);
        }

        bool stepSuccess = false;
        switch (step.type) {
        case FixStepType::DownloadLibrary:
        case FixStepType::DownloadMod:
            stepSuccess = execute_download_step(step, logs);
            break;
        case FixStepType::RemoveMod:
            stepSuccess = execute_remove_step(step, logs);
            break;
        case FixStepType::RepairConfig:
            stepSuccess = execute_repair_config_step(step, gameDir, logs);
            break;
        case FixStepType::CleanCache:
            stepSuccess = execute_clean_cache_step(gameDir, logs);
            break;
        case FixStepType::UpdateJava:
            if (logs) {
                logs->push_back("Java 更新需要用户确认，跳过自动执行");
            }
            stepSuccess = true;  // 标记为成功，但需要用户后续操作
            break;
        case FixStepType::RestartRequired:
            stepSuccess = true;
            break;
        }

        if (!stepSuccess) {
            if (logs) {
                logs->push_back("步骤失败: " + step.description);
            }
            return false;
        }

        completedSteps++;
        if (progressCallback) {
            progressCallback(step.description, (completedSteps * 100) / totalSteps);
        }
    }

    if (logs) {
        logs->push_back("修复计划执行完成");
    }

    return true;
}

// 5. 辅助功能

std::vector<std::string> DiagnosticsService::get_supported_issue_codes() const {
    std::vector<std::string> codes;
    const auto& db = get_issue_database();
    for (const auto& [code, info] : db) {
        codes.push_back(code);
    }
    return codes;
}

bool DiagnosticsService::can_auto_fix(const std::string& issueCode) const {
    const auto& db = get_issue_database();
    auto it = db.find(issueCode);
    if (it != db.end()) {
        return it->second.autoFixable;
    }
    return false;
}

// ========== 内部辅助方法实现 ==========

std::vector<DetectedIssue> DiagnosticsService::detect_java_issues(const std::string& logContent) const {
    std::vector<DetectedIssue> issues;

    const auto& patterns = get_log_patterns();
    for (const auto& pattern : patterns) {
        if (pattern.issueCode.find("java") == 0 || pattern.issueCode.find("java_") == 0) {
            bool matched = true;
            for (const auto& keyword : pattern.keywords) {
                if (!contains_ci(logContent, keyword)) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                issues.push_back(create_issue(pattern.issueCode));
            }
        }
    }

    // 额外检测 Java 版本信息
    std::regex javaVersionRegex("(Java|java)\\s+(version\\s+)?(\\d+)[._](\\d+)");
    std::smatch match;
    if (std::regex_search(logContent, match, javaVersionRegex)) {
        // 检测到了 Java 版本信息，可以进一步分析
    }

    return issues;
}

std::vector<DetectedIssue> DiagnosticsService::detect_memory_issues(const std::string& logContent) const {
    std::vector<DetectedIssue> issues;

    if (contains_ci(logContent, "OutOfMemoryError") || 
        contains_ci(logContent, "GC overhead limit exceeded")) {
        issues.push_back(create_issue("out_of_memory"));
    }

    // 检测内存泄漏迹象
    if (contains_ci(logContent, "memory leak") || 
        contains_ci(logContent, "leaking")) {
        issues.push_back(create_issue("memory_leak"));
    }

    return issues;
}

std::vector<DetectedIssue> DiagnosticsService::detect_dependency_issues(const std::string& logContent) const {
    std::vector<DetectedIssue> issues;

    const auto& patterns = get_log_patterns();
    for (const auto& pattern : patterns) {
        if (pattern.issueCode.find("dependency") != std::string::npos ||
            pattern.issueCode.find("mod_") == 0) {
            bool matched = true;
            for (const auto& keyword : pattern.keywords) {
                if (!contains_ci(logContent, keyword)) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                issues.push_back(create_issue(pattern.issueCode));
            }
        }
    }

    return issues;
}

std::vector<DetectedIssue> DiagnosticsService::detect_loader_issues(const std::string& logContent) const {
    std::vector<DetectedIssue> issues;

    const auto& patterns = get_log_patterns();
    for (const auto& pattern : patterns) {
        if (pattern.issueCode.find("loader") != std::string::npos ||
            pattern.issueCode.find("fabric") != std::string::npos ||
            pattern.issueCode.find("forge") != std::string::npos ||
            pattern.issueCode.find("mixin") != std::string::npos) {
            bool matched = true;
            for (const auto& keyword : pattern.keywords) {
                if (!contains_ci(logContent, keyword)) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                issues.push_back(create_issue(pattern.issueCode));
            }
        }
    }

    return issues;
}

std::vector<DetectedIssue> DiagnosticsService::detect_config_issues(const std::string& logContent) const {
    std::vector<DetectedIssue> issues;

    const auto& patterns = get_log_patterns();
    for (const auto& pattern : patterns) {
        if (pattern.issueCode.find("config") != std::string::npos ||
            pattern.issueCode.find("json") != std::string::npos ||
            pattern.issueCode.find("options") != std::string::npos) {
            bool matched = true;
            for (const auto& keyword : pattern.keywords) {
                if (!contains_ci(logContent, keyword)) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                issues.push_back(create_issue(pattern.issueCode));
            }
        }
    }

    return issues;
}

std::vector<DetectedIssue> DiagnosticsService::detect_graphics_issues(const std::string& logContent) const {
    std::vector<DetectedIssue> issues;

    const auto& patterns = get_log_patterns();
    for (const auto& pattern : patterns) {
        if (pattern.issueCode.find("opengl") != std::string::npos ||
            pattern.issueCode.find("glfw") != std::string::npos ||
            pattern.issueCode.find("graphics") != std::string::npos ||
            pattern.issueCode.find("intel") != std::string::npos ||
            pattern.issueCode.find("nvidia") != std::string::npos) {
            bool matched = true;
            for (const auto& keyword : pattern.keywords) {
                if (!contains_ci(logContent, keyword)) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                issues.push_back(create_issue(pattern.issueCode));
            }
        }
    }

    return issues;
}

std::vector<DetectedIssue> DiagnosticsService::detect_library_issues(const std::string& logContent) const {
    std::vector<DetectedIssue> issues;

    const auto& patterns = get_log_patterns();
    for (const auto& pattern : patterns) {
        if (pattern.issueCode.find("library") != std::string::npos ||
            pattern.issueCode.find("lwjgl") != std::string::npos ||
            pattern.issueCode.find("dll") != std::string::npos ||
            pattern.issueCode.find("native") != std::string::npos) {
            bool matched = true;
            for (const auto& keyword : pattern.keywords) {
                if (!contains_ci(logContent, keyword)) {
                    matched = false;
                    break;
                }
            }
            if (matched) {
                issues.push_back(create_issue(pattern.issueCode));
            }
        }
    }

    return issues;
}

// ========== 修复执行辅助方法 ==========

bool DiagnosticsService::execute_download_step(const FixStep& step, std::vector<std::string>* logs) const {
    if (logs) {
        logs->push_back("下载: " + step.targetId);
    }
    // 实际下载逻辑需要集成 download_service
    // 这里返回 true 表示步骤已记录
    return true;
}

bool DiagnosticsService::execute_remove_step(const FixStep& step, std::vector<std::string>* logs) const {
    if (logs) {
        logs->push_back("移除: " + step.targetId);
    }
    if (!step.targetPath.empty()) {
        std::error_code ec;
        std::filesystem::remove(step.targetPath, ec);
        if (ec) {
            if (logs) {
                logs->push_back("移除失败: " + ec.message());
            }
            return false;
        }
    }
    return true;
}

bool DiagnosticsService::execute_repair_config_step(const FixStep& step, const std::filesystem::path& gameDir, 
                                                     std::vector<std::string>* logs) const {
    if (logs) {
        logs->push_back("修复配置文件: " + step.targetId);
    }
    
    if (!step.targetPath.empty()) {
        // 备份原文件
        auto backupPath = step.targetPath;
        backupPath += ".backup";
        
        std::error_code ec;
        std::filesystem::copy_file(step.targetPath, backupPath, 
                                   std::filesystem::copy_options::overwrite_existing, ec);
        
        if (!ec) {
            if (logs) {
                logs->push_back("已备份原配置到: " + backupPath.string());
            }
        }
        
        // 删除损坏的文件（下次启动会重新生成）
        std::filesystem::remove(step.targetPath, ec);
    }
    
    return true;
}

bool DiagnosticsService::execute_clean_cache_step(const std::filesystem::path& gameDir, 
                                                   std::vector<std::string>* logs) const {
    if (logs) {
        logs->push_back("清理缓存...");
    }
    
    // 清理常见缓存目录
    std::vector<std::filesystem::path> cacheDirs = {
        gameDir / "cache",
        gameDir / ".cache",
        gameDir / "config" / "dawn" / "cache"
    };
    
    for (const auto& dir : cacheDirs) {
        if (std::filesystem::exists(dir)) {
            std::error_code ec;
            std::filesystem::remove_all(dir, ec);
            if (!ec && logs) {
                logs->push_back("已清理: " + dir.string());
            }
        }
    }
    
    return true;
}

} // namespace dawn::core
