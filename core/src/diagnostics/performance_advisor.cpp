#include "dawn/core/diagnostics/performance_advisor.h"

#include <algorithm>
#include <fstream>
#include <regex>
#include <sstream>

namespace dawn::core {

// ========== Utility Functions ==========

std::string instance_profile_type_to_string(InstanceProfileType type) {
    switch (type) {
    case InstanceProfileType::Vanilla: return "vanilla";
    case InstanceProfileType::LightModpack: return "light";
    case InstanceProfileType::HeavyModpack: return "heavy";
    case InstanceProfileType::ShaderFocused: return "shader";
    case InstanceProfileType::Unknown: return "unknown";
    }
    return "unknown";
}

InstanceProfileType instance_profile_type_from_string(const std::string& type) {
    if (type == "vanilla") return InstanceProfileType::Vanilla;
    if (type == "light") return InstanceProfileType::LightModpack;
    if (type == "heavy") return InstanceProfileType::HeavyModpack;
    if (type == "shader") return InstanceProfileType::ShaderFocused;
    return InstanceProfileType::Unknown;
}

// ========== Constructor ==========

PerformanceAdvisor::PerformanceAdvisor() = default;

// ========== Performance Analysis ==========

PerformanceProfile PerformanceAdvisor::analyze_instance_performance(
    const std::string& instanceId,
    const InstanceManifest& manifest) const {

    std::filesystem::path instanceDir(manifest.gameDir);
    InstanceProfileType detectedType = detect_instance_type(manifest, instanceDir);
    PerformanceProfile profile = get_preset_profile(detectedType);
    InstanceAnalysis analysis = analyze_instance(instanceId, instanceDir);

    if (analysis.modCount > 0 && detectedType == InstanceProfileType::Vanilla) {
        detectedType = InstanceProfileType::LightModpack;
        profile = get_preset_profile(detectedType);
    }

    profile.warnings = generate_warnings(manifest, instanceDir);

    std::vector<std::string> missingMods = check_missing_recommended_mods(manifest, instanceDir);
    if (!missingMods.empty()) {
        profile.recommendedMods.insert(profile.recommendedMods.end(),
                                       missingMods.begin(), missingMods.end());
    }

    return profile;
}

InstanceAnalysis PerformanceAdvisor::analyze_instance(
    const std::string& instanceId,
    const std::filesystem::path& instanceDir) const {

    InstanceAnalysis analysis;
    analysis.instanceId = instanceId;

    std::filesystem::path modsDir = instanceDir / "mods";
    std::filesystem::path shaderpacksDir = instanceDir / "shaderpacks";
    std::filesystem::path resourcepacksDir = instanceDir / "resourcepacks";

    analysis.modCount = count_mods(modsDir);
    analysis.shaderCount = count_shaders(shaderpacksDir);
    analysis.resourcePackCount = count_resource_packs(resourcepacksDir);

    analysis.hasOptiFine = detect_optifine(modsDir);
    analysis.hasSodium = detect_sodium(modsDir);
    analysis.hasIris = detect_iris(modsDir);

    if (analysis.hasOptiFine) analysis.detectedFeatures.push_back("OptiFine");
    if (analysis.hasSodium) analysis.detectedFeatures.push_back("Sodium");
    if (analysis.hasIris) analysis.detectedFeatures.push_back("Iris");
    if (analysis.shaderCount > 0) analysis.detectedFeatures.push_back("Shaders");

    if (analysis.modCount == 0 && !analysis.hasOptiFine) {
        analysis.detectedType = InstanceProfileType::Vanilla;
        analysis.detectedTypeName = "Vanilla";
    } else if (analysis.shaderCount > 0 || analysis.hasOptiFine) {
        analysis.detectedType = InstanceProfileType::ShaderFocused;
        analysis.detectedTypeName = "Shader Focused";
    } else if (analysis.modCount > 100) {
        analysis.detectedType = InstanceProfileType::HeavyModpack;
        analysis.detectedTypeName = "Heavy Modpack";
    } else if (analysis.modCount > 0) {
        analysis.detectedType = InstanceProfileType::LightModpack;
        analysis.detectedTypeName = "Light Modpack";
    } else {
        analysis.detectedType = InstanceProfileType::Unknown;
        analysis.detectedTypeName = "Unknown";
    }

    return analysis;
}

InstanceProfileType PerformanceAdvisor::detect_instance_type(
    const InstanceManifest& manifest,
    const std::filesystem::path& instanceDir) const {

    InstanceAnalysis analysis = analyze_instance(manifest.id, instanceDir);
    return analysis.detectedType;
}

// ========== Memory Recommendations ==========

MemoryRecommendation PerformanceAdvisor::recommend_memory(
    const InstanceManifest& manifest,
    const InstanceProfileType profileType) const {

    MemoryRecommendation rec;
    rec.currentMemory = 4096;

    if (!manifest.memoryProfile.empty()) {
        std::regex memRegex(R"(-Xmx(\d+)([GgMm]))");
        std::smatch match;
        if (std::regex_search(manifest.memoryProfile, match, memRegex)) {
            int value = std::stoi(match[1].str());
            char unit = std::tolower(match[2].str()[0]);
            rec.currentMemory = (unit == 'g') ? value * 1024 : value;
        }
    }

    switch (profileType) {
    case InstanceProfileType::Vanilla:
        rec.recommendedMin = 2048;
        rec.recommendedMax = 4096;
        rec.recommendedOptimal = 3072;
        rec.explanation = "Vanilla Minecraft requires less memory, 3GB is sufficient.";
        break;

    case InstanceProfileType::LightModpack:
        rec.recommendedMin = 4096;
        rec.recommendedMax = 6144;
        rec.recommendedOptimal = 4096;
        rec.explanation = "Light modpack with few mods, 4GB recommended.";
        break;

    case InstanceProfileType::HeavyModpack:
        rec.recommendedMin = 6144;
        rec.recommendedMax = 12288;
        rec.recommendedOptimal = 8192;
        rec.explanation = "Heavy modpack with many mods, 8GB recommended for best experience.";
        break;

    case InstanceProfileType::ShaderFocused:
        rec.recommendedMin = 4096;
        rec.recommendedMax = 8192;
        rec.recommendedOptimal = 6144;
        rec.explanation = "Shaders require additional memory, 6GB+ recommended.";
        break;

    default:
        rec.recommendedMin = 4096;
        rec.recommendedMax = 6144;
        rec.recommendedOptimal = 4096;
        rec.explanation = "Unknown instance type, using default configuration.";
        break;
    }

    rec.needsAdjustment = (rec.currentMemory < rec.recommendedMin ||
                           rec.currentMemory > rec.recommendedMax * 2);

    return rec;
}

MemoryRecommendation PerformanceAdvisor::recommend_memory(
    const InstanceAnalysis& analysis) const {

    MemoryRecommendation rec;
    rec.currentMemory = 4096;

    if (analysis.modCount == 0) {
        rec.recommendedMin = 2048;
        rec.recommendedMax = 4096;
        rec.recommendedOptimal = 3072;
        rec.explanation = "Vanilla instance detected, 3GB is sufficient.";
    } else if (analysis.modCount < 50) {
        rec.recommendedMin = 4096;
        rec.recommendedMax = 6144;
        rec.recommendedOptimal = 4096;
        rec.explanation = "Light modpack (" + std::to_string(analysis.modCount) + " mods), 4GB recommended.";
    } else if (analysis.modCount < 150) {
        rec.recommendedMin = 6144;
        rec.recommendedMax = 10240;
        rec.recommendedOptimal = 8192;
        rec.explanation = "Medium modpack (" + std::to_string(analysis.modCount) + " mods), 8GB recommended.";
    } else {
        rec.recommendedMin = 8192;
        rec.recommendedMax = 16384;
        rec.recommendedOptimal = 12288;
        rec.explanation = "Large modpack (" + std::to_string(analysis.modCount) + " mods), 12GB recommended.";
    }

    if (analysis.shaderCount > 0 || analysis.hasOptiFine) {
        rec.recommendedMin += 1024;
        rec.recommendedOptimal += 1024;
        rec.explanation += " Shaders require additional memory.";
    }

    return rec;
}

// ========== JVM Arguments Recommendations ==========

JvmArgsRecommendation PerformanceAdvisor::recommend_jvm_args(
    const InstanceManifest& manifest,
    const InstanceProfileType profileType) const {

    JvmArgsRecommendation rec;

    switch (profileType) {
    case InstanceProfileType::Vanilla:
        rec.recommendedArgs = get_vanilla_jvm_args();
        rec.gcType = "G1GC";
        rec.gcExplanation = "G1GC is suitable for vanilla, providing stable GC performance.";
        break;

    case InstanceProfileType::LightModpack:
        rec.recommendedArgs = get_light_modpack_jvm_args();
        rec.gcType = "G1GC";
        rec.gcExplanation = "G1GC performs well in light modpacks, balancing throughput and latency.";
        break;

    case InstanceProfileType::HeavyModpack:
        rec.recommendedArgs = get_heavy_modpack_jvm_args();
        rec.gcType = "ZGC";
        rec.gcExplanation = "ZGC is suitable for heavy modpacks, ultra-low latency prevents stuttering.";
        break;

    case InstanceProfileType::ShaderFocused:
        rec.recommendedArgs = get_shader_focused_jvm_args();
        rec.gcType = "Shenandoah";
        rec.gcExplanation = "Shenandoah GC provides stable frame rates, suitable for shader scenarios.";
        break;

    default:
        rec.recommendedArgs = get_vanilla_jvm_args();
        rec.gcType = "G1GC";
        rec.gcExplanation = "Using default G1GC configuration.";
        break;
    }

    rec.explanations.push_back("-XX:+Use" + rec.gcType + " - " + rec.gcExplanation);
    rec.explanations.push_back("-XX:+UnlockExperimentalVMOptions - Enable experimental features for better performance");
    rec.explanations.push_back("-XX:+DisableExplicitGC - Prevent mods from incorrectly calling System.gc()");

    return rec;
}

std::string PerformanceAdvisor::recommend_gc_type(const InstanceProfileType profileType) const {
    switch (profileType) {
    case InstanceProfileType::Vanilla:
        return "G1GC";
    case InstanceProfileType::LightModpack:
        return "G1GC";
    case InstanceProfileType::HeavyModpack:
        return "ZGC";
    case InstanceProfileType::ShaderFocused:
        return "Shenandoah";
    default:
        return "G1GC";
    }
}

// ========== Mod Recommendations ==========

std::vector<std::string> PerformanceAdvisor::recommend_optimization_mods(
    const InstanceManifest& manifest,
    const InstanceProfileType profileType) const {

    switch (profileType) {
    case InstanceProfileType::Vanilla:
        return get_vanilla_recommended_mods();
    case InstanceProfileType::LightModpack:
        return get_light_recommended_mods();
    case InstanceProfileType::HeavyModpack:
        return get_heavy_recommended_mods();
    case InstanceProfileType::ShaderFocused:
        return get_shader_recommended_mods();
    default:
        return {};
    }
}

std::vector<std::string> PerformanceAdvisor::check_missing_recommended_mods(
    const InstanceManifest& manifest,
    const std::filesystem::path& instanceDir) const {

    std::vector<std::string> missing;
    std::filesystem::path modsDir = instanceDir / "mods";

    InstanceProfileType profileType = detect_instance_type(manifest, instanceDir);
    std::vector<std::string> recommended = recommend_optimization_mods(manifest, profileType);

    for (const auto& modId : recommended) {
        if (!has_mod_by_id(modsDir, modId)) {
            missing.push_back(modId);
        }
    }

    return missing;
}

// ========== Update Simulation ==========

UpdateSimulationResult PerformanceAdvisor::simulate_update(
    const std::string& instanceId,
    const std::vector<ContentUpdateItem>& contentUpdates) const {

    UpdateSimulationResult result;
    result.affectedInstances.push_back(instanceId);

    for (const auto& item : contentUpdates) {
        UpdateImpact impact = simulate_single_update(
            item.contentId,
            item.currentVersionId,
            item.targetVersionId);
        impact.contentType = item.contentType;

        result.impacts.push_back(impact);

        if (impact.hasBreakingChanges) {
            result.hasBreakingChanges = true;
        }
    }

    result.summary = generate_update_summary(result.impacts);

    if (result.hasBreakingChanges) {
        result.recommendsBackup = true;
        result.warnings.push_back("Breaking changes detected, backup recommended.");
    }

    result.estimatedTotalDownloadSize = estimate_download_size(contentUpdates);

    if (result.impacts.size() > 5) {
        result.recommendations.push_back("Many updates, recommend doing during idle time.");
    }
    if (result.hasBreakingChanges) {
        result.recommendations.push_back("Please read changelog carefully before updating.");
    }

    return result;
}

UpdateImpact PerformanceAdvisor::simulate_single_update(
    const std::string& contentId,
    const std::string& currentVersion,
    const std::string& targetVersion) const {

    UpdateImpact impact;
    impact.contentId = contentId;
    impact.currentVersion = currentVersion;
    impact.targetVersion = targetVersion;

    impact.dependencyChanges = analyze_dependency_changes(contentId, currentVersion, targetVersion);
    impact.hasBreakingChanges = check_breaking_changes(contentId, currentVersion, targetVersion);
    impact.estimatedDownloadSize = "~2-5 MB";
    impact.canAutoRollback = !impact.hasBreakingChanges;

    return impact;
}

std::vector<UpdateSimulationResult> PerformanceAdvisor::simulate_batch_updates(
    const std::vector<std::pair<std::string, std::vector<ContentUpdateItem>>>& updates) const {

    std::vector<UpdateSimulationResult> results;

    for (const auto& [instanceId, items] : updates) {
        results.push_back(simulate_update(instanceId, items));
    }

    return results;
}

// ========== Preset Profiles ==========

PerformanceProfile PerformanceAdvisor::get_preset_profile(const InstanceProfileType type) const {
    PerformanceProfile profile;
    profile.profileType = instance_profile_type_to_string(type);

    switch (type) {
    case InstanceProfileType::Vanilla:
        profile.profileName = "Vanilla";
        profile.description = "Original Minecraft, no mods or only few helper mods";
        profile.recommendedMemoryMin = 2048;
        profile.recommendedMemoryMax = 4096;
        profile.suggestedJvmArgs = get_vanilla_jvm_args();
        profile.recommendedGc = "G1GC";
        profile.recommendedMods = get_vanilla_recommended_mods();
        break;

    case InstanceProfileType::LightModpack:
        profile.profileName = "Light Modpack";
        profile.description = "Contains few optimization and functional mods (<50)";
        profile.recommendedMemoryMin = 4096;
        profile.recommendedMemoryMax = 6144;
        profile.suggestedJvmArgs = get_light_modpack_jvm_args();
        profile.recommendedGc = "G1GC";
        profile.recommendedMods = get_light_recommended_mods();
        break;

    case InstanceProfileType::HeavyModpack:
        profile.profileName = "Heavy Modpack";
        profile.description = "Large modpack with many mods (>100)";
        profile.recommendedMemoryMin = 6144;
        profile.recommendedMemoryMax = 12288;
        profile.suggestedJvmArgs = get_heavy_modpack_jvm_args();
        profile.recommendedGc = "ZGC";
        profile.recommendedMods = get_heavy_recommended_mods();
        break;

    case InstanceProfileType::ShaderFocused:
        profile.profileName = "Shader Focused";
        profile.description = "Instance focused on shader effects";
        profile.recommendedMemoryMin = 4096;
        profile.recommendedMemoryMax = 8192;
        profile.suggestedJvmArgs = get_shader_focused_jvm_args();
        profile.recommendedGc = "Shenandoah";
        profile.recommendedMods = get_shader_recommended_mods();
        break;

    default:
        profile.profileName = "Default";
        profile.description = "General configuration";
        profile.recommendedMemoryMin = 4096;
        profile.recommendedMemoryMax = 6144;
        profile.suggestedJvmArgs = get_vanilla_jvm_args();
        profile.recommendedGc = "G1GC";
        break;
    }

    return profile;
}

std::vector<PerformanceProfile> PerformanceAdvisor::get_all_preset_profiles() const {
    return {
        get_preset_profile(InstanceProfileType::Vanilla),
        get_preset_profile(InstanceProfileType::LightModpack),
        get_preset_profile(InstanceProfileType::HeavyModpack),
        get_preset_profile(InstanceProfileType::ShaderFocused)
    };
}

// ========== Helper Functions ==========

std::vector<std::string> PerformanceAdvisor::generate_warnings(
    const InstanceManifest& manifest,
    const std::filesystem::path& instanceDir) const {

    std::vector<std::string> warnings;
    InstanceAnalysis analysis = analyze_instance(manifest.id, instanceDir);

    MemoryRecommendation memRec = recommend_memory(analysis);
    if (memRec.currentMemory < memRec.recommendedMin) {
        warnings.push_back("Current memory may be insufficient, recommend at least " +
                          std::to_string(memRec.recommendedMin / 1024) + "GB");
    }

    if (analysis.hasOptiFine && analysis.hasSodium) {
        warnings.push_back("OptiFine and Sodium detected together, may cause conflicts.");
    }

    if (analysis.shaderCount > 0 && !analysis.hasOptiFine && !analysis.hasIris) {
        warnings.push_back("Shaders detected but OptiFine or Iris not installed, shaders may not work.");
    }

    if (analysis.modCount > 200) {
        warnings.push_back("Many mods (" + std::to_string(analysis.modCount) +
                          "), startup time may be longer.");
    }

    try {
        auto space = std::filesystem::space(instanceDir);
        if (space.available < 1024 * 1024 * 1024) {
            warnings.push_back("Low disk space, may affect game performance.");
        }
    } catch (...) {
        // Ignore error
    }

    return warnings;
}

std::string PerformanceAdvisor::generate_performance_report(
    const std::string& instanceId,
    const InstanceManifest& manifest) const {

    std::ostringstream report;
    std::filesystem::path instanceDir(manifest.gameDir);

    PerformanceProfile profile = analyze_instance_performance(instanceId, manifest);
    InstanceAnalysis analysis = analyze_instance(instanceId, instanceDir);
    MemoryRecommendation memRec = recommend_memory(analysis);
    JvmArgsRecommendation jvmRec = recommend_jvm_args(manifest, analysis.detectedType);

    report << "=== Dawn Performance Report ===\n\n";
    report << "Instance: " << manifest.name << " (" << instanceId << ")\n";
    report << "Type: " << analysis.detectedTypeName << "\n";
    report << "MC Version: " << manifest.mcVersion << "\n\n";

    report << "--- Resource Stats ---\n";
    report << "Mod Count: " << analysis.modCount << "\n";
    report << "Shader Count: " << analysis.shaderCount << "\n";
    report << "Resource Pack Count: " << analysis.resourcePackCount << "\n";

    report << "\n--- Memory Recommendation ---\n";
    report << "Min Recommended: " << memRec.recommendedMin / 1024 << " GB\n";
    report << "Max Recommended: " << memRec.recommendedMax / 1024 << " GB\n";
    report << "Optimal: " << memRec.recommendedOptimal / 1024 << " GB\n";
    report << "Note: " << memRec.explanation << "\n";

    report << "\n--- JVM Args Recommendation ---\n";
    report << "Recommended GC: " << jvmRec.gcType << "\n";
    report << "GC Note: " << jvmRec.gcExplanation << "\n";
    report << "Suggested Args:\n";
    for (const auto& arg : jvmRec.recommendedArgs) {
        report << "  " << arg << "\n";
    }

    if (!profile.recommendedMods.empty()) {
        report << "\n--- Recommended Optimization Mods ---\n";
        for (const auto& mod : profile.recommendedMods) {
            report << "  - " << mod << "\n";
        }
    }

    if (!profile.warnings.empty()) {
        report << "\n--- Warnings ---\n";
        for (const auto& warning : profile.warnings) {
            report << "  ! " << warning << "\n";
        }
    }

    return report.str();
}

bool PerformanceAdvisor::validate_configuration(
    const InstanceManifest& manifest,
    std::vector<std::string>* issues) const {

    bool valid = true;

    if (manifest.id.empty()) {
        if (issues) issues->push_back("Instance ID cannot be empty");
        valid = false;
    }

    if (manifest.mcVersion.empty()) {
        if (issues) issues->push_back("Minecraft version cannot be empty");
        valid = false;
    }

    if (manifest.gameDir.empty()) {
        if (issues) issues->push_back("Game directory cannot be empty");
        valid = false;
    }

    if (!std::filesystem::exists(manifest.gameDir)) {
        if (issues) issues->push_back("Game directory does not exist: " + manifest.gameDir);
        valid = false;
    }

    return valid;
}

// ========== Internal Helper Methods ==========

int PerformanceAdvisor::count_mods(const std::filesystem::path& modsDir) const {
    if (!std::filesystem::exists(modsDir)) return 0;

    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(modsDir)) {
        if (entry.is_regular_file()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".jar" || ext == ".zip") {
                count++;
            }
        }
    }
    return count;
}

int PerformanceAdvisor::count_shaders(const std::filesystem::path& shaderpacksDir) const {
    if (!std::filesystem::exists(shaderpacksDir)) return 0;

    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(shaderpacksDir)) {
        if (entry.is_regular_file() || entry.is_directory()) {
            count++;
        }
    }
    return count;
}

int PerformanceAdvisor::count_resource_packs(const std::filesystem::path& resourcepacksDir) const {
    if (!std::filesystem::exists(resourcepacksDir)) return 0;

    int count = 0;
    for (const auto& entry : std::filesystem::directory_iterator(resourcepacksDir)) {
        if (entry.is_regular_file() || entry.is_directory()) {
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
            if (ext == ".zip" || entry.is_directory()) {
                count++;
            }
        }
    }
    return count;
}

bool PerformanceAdvisor::detect_optifine(const std::filesystem::path& modsDir) const {
    return has_mod_by_id(modsDir, "optifine");
}

bool PerformanceAdvisor::detect_sodium(const std::filesystem::path& modsDir) const {
    return has_mod_by_id(modsDir, "sodium");
}

bool PerformanceAdvisor::detect_iris(const std::filesystem::path& modsDir) const {
    return has_mod_by_id(modsDir, "iris");
}

bool PerformanceAdvisor::has_mod_by_id(const std::filesystem::path& modsDir, const std::string& modId) const {
    if (!std::filesystem::exists(modsDir)) return false;

    std::string lowerModId = modId;
    std::transform(lowerModId.begin(), lowerModId.end(), lowerModId.begin(), ::tolower);

    for (const auto& entry : std::filesystem::directory_iterator(modsDir)) {
        if (entry.is_regular_file()) {
            std::string filename = entry.path().filename().string();
            std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
            if (filename.find(lowerModId) != std::string::npos) {
                return true;
            }
        }
    }
    return false;
}

std::vector<std::string> PerformanceAdvisor::get_vanilla_jvm_args() const {
    return {
        "-XX:+UnlockExperimentalVMOptions",
        "-XX:+UseG1GC",
        "-XX:G1NewSizePercent=20",
        "-XX:G1ReservePercent=20",
        "-XX:MaxGCPauseMillis=50",
        "-XX:G1HeapRegionSize=16M",
        "-XX:+DisableExplicitGC"
    };
}

std::vector<std::string> PerformanceAdvisor::get_light_modpack_jvm_args() const {
    return {
        "-XX:+UnlockExperimentalVMOptions",
        "-XX:+UseG1GC",
        "-XX:G1NewSizePercent=20",
        "-XX:G1ReservePercent=20",
        "-XX:MaxGCPauseMillis=50",
        "-XX:G1HeapRegionSize=16M",
        "-XX:+DisableExplicitGC",
        "-XX:+UseStringDeduplication",
        "-XX:+OptimizeStringConcat"
    };
}

std::vector<std::string> PerformanceAdvisor::get_heavy_modpack_jvm_args() const {
    return {
        "-XX:+UnlockExperimentalVMOptions",
        "-XX:+UseZGC",
        "-XX:+DisableExplicitGC",
        "-XX:+UseStringDeduplication",
        "-XX:+OptimizeStringConcat",
        "-XX:+UseLargePages",
        "-XX:LargePageSizeInBytes=2M",
        "-XX:+AlwaysPreTouch"
    };
}

std::vector<std::string> PerformanceAdvisor::get_shader_focused_jvm_args() const {
    return {
        "-XX:+UnlockExperimentalVMOptions",
        "-XX:+UseShenandoahGC",
        "-XX:ShenandoahGCHeuristics=compact",
        "-XX:+DisableExplicitGC",
        "-XX:+UseStringDeduplication",
        "-XX:+OptimizeStringConcat",
        "-Dfml.ignoreInvalidMinecraftCertificates=true",
        "-Dfml.ignorePatchDiscrepancies=true"
    };
}

std::vector<std::string> PerformanceAdvisor::get_vanilla_recommended_mods() const {
    return {
        "sodium",
        "lithium",
        "starlight",
        "ferritecore",
        "lazydfu",
        "krypton",
        "entityculling"
    };
}

std::vector<std::string> PerformanceAdvisor::get_light_recommended_mods() const {
    return {
        "sodium",
        "lithium",
        "starlight",
        "ferritecore",
        "lazydfu",
        "krypton",
        "entityculling",
        "modernfix",
        "memoryleakfix",
        "smoothboot"
    };
}

std::vector<std::string> PerformanceAdvisor::get_heavy_recommended_mods() const {
    return {
        "sodium",
        "lithium",
        "starlight",
        "ferritecore",
        "lazydfu",
        "krypton",
        "entityculling",
        "modernfix",
        "memoryleakfix",
        "smoothboot",
        "spark",
        "observable",
        "debugify",
        "c2me",
        "vmp"
    };
}

std::vector<std::string> PerformanceAdvisor::get_shader_recommended_mods() const {
    return {
        "sodium",
        "iris",
        "lithium",
        "starlight",
        "ferritecore",
        "lazydfu",
        "entityculling",
        "reeses-sodium-options",
        "sodium-extra",
        "irisshaders"
    };
}

std::vector<std::string> PerformanceAdvisor::analyze_dependency_changes(
    const std::string& contentId,
    const std::string& currentVersion,
    const std::string& targetVersion) const {

    std::vector<std::string> changes;

    auto parse_version = [](const std::string& ver) -> std::vector<int> {
        std::vector<int> parts;
        std::stringstream ss(ver);
        std::string part;
        while (std::getline(ss, part, '.')) {
            try {
                parts.push_back(std::stoi(part));
            } catch (...) {
                parts.push_back(0);
            }
        }
        return parts;
    };

    auto current = parse_version(currentVersion);
    auto target = parse_version(targetVersion);

    if (!current.empty() && !target.empty() && target[0] > current[0]) {
        changes.push_back("Major version upgrade, API changes may be introduced");
    }

    return changes;
}

bool PerformanceAdvisor::check_breaking_changes(
    const std::string& contentId,
    const std::string& currentVersion,
    const std::string& targetVersion) const {

    auto parse_version = [](const std::string& ver) -> std::vector<int> {
        std::vector<int> parts;
        std::stringstream ss(ver);
        std::string part;
        while (std::getline(ss, part, '.')) {
            try {
                parts.push_back(std::stoi(part));
            } catch (...) {
                parts.push_back(0);
            }
        }
        return parts;
    };

    auto current = parse_version(currentVersion);
    auto target = parse_version(targetVersion);

    if (!current.empty() && !target.empty()) {
        if (target[0] > current[0]) {
            return true;
        }
    }

    return false;
}

std::string PerformanceAdvisor::estimate_download_size(
    const std::vector<ContentUpdateItem>& updates) const {

    size_t totalItems = updates.size();
    size_t estimatedMin = totalItems * 2;
    size_t estimatedMax = totalItems * 5;

    if (estimatedMax < 1024) {
        return std::to_string(estimatedMin) + " - " + std::to_string(estimatedMax) + " MB";
    } else {
        return std::to_string(estimatedMin / 1024) + "." +
               std::to_string((estimatedMin % 1024) / 100) + " - " +
               std::to_string(estimatedMax / 1024) + "." +
               std::to_string((estimatedMax % 1024) / 100) + " GB";
    }
}

std::string PerformanceAdvisor::generate_update_summary(
    const std::vector<UpdateImpact>& impacts) const {

    int total = impacts.size();
    int breaking = 0;
    int safe = 0;

    for (const auto& impact : impacts) {
        if (impact.hasBreakingChanges) {
            breaking++;
        } else {
            safe++;
        }
    }

    std::ostringstream summary;
    summary << "Will update " << total << " items";
    if (breaking > 0) {
        summary << ", " << breaking << " with breaking changes";
    }
    if (safe > 0) {
        summary << ", " << safe << " safe to update";
    }

    return summary.str();
}

} // namespace dawn::core
