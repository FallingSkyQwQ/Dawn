#pragma once

#include "dawn/core/model/instance_manifest.h"

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace dawn::core {

// Instance type enumeration
enum class InstanceProfileType {
    Vanilla,        // Pure version
    LightModpack,   // Light modpack
    HeavyModpack,   // Heavy modpack
    ShaderFocused,  // Shader-focused instance
    Unknown,        // Unknown type
};

// Performance profile configuration
struct PerformanceProfile {
    std::string profileType;              // "vanilla", "light", "heavy", "shader"
    std::string profileName;              // User-friendly name
    std::string description;              // Description
    int recommendedMemoryMin = 2048;      // Minimum memory (MB)
    int recommendedMemoryMax = 4096;      // Maximum memory (MB)
    std::vector<std::string> suggestedJvmArgs;  // Suggested JVM arguments
    std::string recommendedGc;            // Recommended GC type
    std::vector<std::string> recommendedMods;   // Recommended optimization mods
    std::vector<std::string> warnings;    // Warning messages
};

// Instance analysis result
struct InstanceAnalysis {
    std::string instanceId;
    InstanceProfileType detectedType = InstanceProfileType::Unknown;
    std::string detectedTypeName;
    int modCount = 0;
    int shaderCount = 0;
    int resourcePackCount = 0;
    std::string loaderType;
    bool hasOptiFine = false;
    bool hasSodium = false;
    bool hasIris = false;
    std::vector<std::string> detectedFeatures;  // Detected features
};

// Update impact
struct UpdateImpact {
    std::string contentId;
    std::string contentName;
    std::string contentType;              // "mod", "resourcepack", "shader"
    std::string currentVersion;
    std::string targetVersion;
    bool hasBreakingChanges = false;
    std::vector<std::string> dependencyChanges;
    std::vector<std::string> addedDependencies;
    std::vector<std::string> removedDependencies;
    std::vector<std::string> conflicts;
    bool canAutoRollback = true;
    std::string estimatedDownloadSize;
    std::string changelog;                // Changelog summary
};

// Update simulation result
struct UpdateSimulationResult {
    std::vector<UpdateImpact> impacts;
    std::string summary;
    bool hasBreakingChanges = false;
    bool recommendsBackup = false;
    std::string estimatedTotalDownloadSize;
    std::vector<std::string> affectedInstances;
    std::vector<std::string> warnings;
    std::vector<std::string> recommendations;
};

// Content update item
struct ContentUpdateItem {
    std::string contentId;
    std::string currentVersionId;
    std::string targetVersionId;
    std::string contentType;  // "mod", "resourcepack", "shader"
};

// Memory recommendation
struct MemoryRecommendation {
    int currentMemory;
    int recommendedMin;
    int recommendedMax;
    int recommendedOptimal;
    std::string explanation;
    bool needsAdjustment = false;
};

// JVM arguments recommendation
struct JvmArgsRecommendation {
    std::vector<std::string> recommendedArgs;
    std::vector<std::string> explanations;
    std::string gcType;
    std::string gcExplanation;
};

// Performance advisor service
class PerformanceAdvisor {
public:
    PerformanceAdvisor();
    ~PerformanceAdvisor() = default;

    // ========== Performance Analysis ==========

    // Analyze instance performance and provide recommendations
    PerformanceProfile analyze_instance_performance(
        const std::string& instanceId,
        const InstanceManifest& manifest) const;

    // Analyze instance details
    InstanceAnalysis analyze_instance(
        const std::string& instanceId,
        const std::filesystem::path& instanceDir) const;

    // Detect instance type
    InstanceProfileType detect_instance_type(
        const InstanceManifest& manifest,
        const std::filesystem::path& instanceDir) const;

    // ========== Memory Recommendations ==========

    // Get memory recommendation
    MemoryRecommendation recommend_memory(
        const InstanceManifest& manifest,
        const InstanceProfileType profileType) const;

    // Get memory recommendation (based on analysis result)
    MemoryRecommendation recommend_memory(
        const InstanceAnalysis& analysis) const;

    // ========== JVM Arguments Recommendations ==========

    // Get JVM arguments recommendation
    JvmArgsRecommendation recommend_jvm_args(
        const InstanceManifest& manifest,
        const InstanceProfileType profileType) const;

    // Get GC recommendation
    std::string recommend_gc_type(const InstanceProfileType profileType) const;

    // ========== Mod Recommendations ==========

    // Get recommended optimization mods
    std::vector<std::string> recommend_optimization_mods(
        const InstanceManifest& manifest,
        const InstanceProfileType profileType) const;

    // Check missing recommended mods
    std::vector<std::string> check_missing_recommended_mods(
        const InstanceManifest& manifest,
        const std::filesystem::path& instanceDir) const;

    // ========== Update Simulation ==========

    // Simulate update impact
    UpdateSimulationResult simulate_update(
        const std::string& instanceId,
        const std::vector<ContentUpdateItem>& contentUpdates) const;

    // Simulate single content update
    UpdateImpact simulate_single_update(
        const std::string& contentId,
        const std::string& currentVersion,
        const std::string& targetVersion) const;

    // Batch simulate updates for multiple instances
    std::vector<UpdateSimulationResult> simulate_batch_updates(
        const std::vector<std::pair<std::string, std::vector<ContentUpdateItem>>>& updates) const;

    // ========== Preset Profiles ==========

    // Get preset performance profile
    PerformanceProfile get_preset_profile(const InstanceProfileType type) const;

    // Get all preset profiles
    std::vector<PerformanceProfile> get_all_preset_profiles() const;

    // ========== Helper Functions ==========

    // Check if warnings are needed
    std::vector<std::string> generate_warnings(
        const InstanceManifest& manifest,
        const std::filesystem::path& instanceDir) const;

    // Generate performance report
    std::string generate_performance_report(
        const std::string& instanceId,
        const InstanceManifest& manifest) const;

    // Validate configuration compatibility
    bool validate_configuration(
        const InstanceManifest& manifest,
        std::vector<std::string>* issues = nullptr) const;

private:
    // Internal helper methods
    int count_mods(const std::filesystem::path& modsDir) const;
    int count_shaders(const std::filesystem::path& shaderpacksDir) const;
    int count_resource_packs(const std::filesystem::path& resourcepacksDir) const;

    bool detect_optifine(const std::filesystem::path& modsDir) const;
    bool detect_sodium(const std::filesystem::path& modsDir) const;
    bool detect_iris(const std::filesystem::path& modsDir) const;

    std::vector<std::string> get_vanilla_jvm_args() const;
    std::vector<std::string> get_light_modpack_jvm_args() const;
    std::vector<std::string> get_heavy_modpack_jvm_args() const;
    std::vector<std::string> get_shader_focused_jvm_args() const;

    std::vector<std::string> get_vanilla_recommended_mods() const;
    std::vector<std::string> get_light_recommended_mods() const;
    std::vector<std::string> get_heavy_recommended_mods() const;
    std::vector<std::string> get_shader_recommended_mods() const;

    bool has_mod_by_id(const std::filesystem::path& modsDir, const std::string& modId) const;

    // Analyze dependency changes
    std::vector<std::string> analyze_dependency_changes(
        const std::string& contentId,
        const std::string& currentVersion,
        const std::string& targetVersion) const;

    // Check breaking changes
    bool check_breaking_changes(
        const std::string& contentId,
        const std::string& currentVersion,
        const std::string& targetVersion) const;

    // Estimate download size
    std::string estimate_download_size(
        const std::vector<ContentUpdateItem>& updates) const;

    // Generate update summary
    std::string generate_update_summary(
        const std::vector<UpdateImpact>& impacts) const;
};

// Utility functions
std::string instance_profile_type_to_string(InstanceProfileType type);
InstanceProfileType instance_profile_type_from_string(const std::string& type);

} // namespace dawn::core
