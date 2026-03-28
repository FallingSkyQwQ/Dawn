#include "dawn/core/interfaces/default_launcher_runtime.h"

#include "dawn/core/minecraft/version_package.h"
#include "dawn/core/minecraft/library_resolver.h"
#include "dawn/core/auth/account_service.h"
#include "dawn/core/java/java_service.h"
#include "dawn/infra/fs/file_system.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace dawn::core {

namespace {

// Constants for directory structure
constexpr std::string_view kVersionsDir = "versions";
constexpr std::string_view kLibrariesDir = "libraries";
constexpr std::string_view kNativesDir = "natives";
constexpr std::string_view kAssetsDir = "assets";
constexpr std::string_view kIndexesDir = "indexes";
constexpr std::string_view kObjectsDir = "objects";
constexpr std::string_view kVirtualDir = "virtual";

// Get the launcher root directory (parent of gameDir)
std::filesystem::path get_launcher_root(const std::filesystem::path& gameDir) {
    if (gameDir.empty()) {
        return std::filesystem::current_path();
    }
    // Instance gameDir is typically at .minecraft or instances/<id>/.minecraft
    // The launcher root is the parent of .minecraft
    if (gameDir.filename() == ".minecraft") {
        return gameDir.parent_path();
    }
    return gameDir;
}

// Get versions directory path
std::filesystem::path get_versions_dir(const std::filesystem::path& gameDir) {
    return gameDir / std::string(kVersionsDir);
}

// Get libraries directory path
std::filesystem::path get_libraries_dir(const std::filesystem::path& gameDir) {
    return gameDir / std::string(kLibrariesDir);
}

// Get natives directory path for a specific version
std::filesystem::path get_natives_dir(const std::filesystem::path& gameDir, const std::string& versionId) {
    return get_versions_dir(gameDir) / versionId / std::string(kNativesDir);
}

// Get assets directory path
std::filesystem::path get_assets_dir(const std::filesystem::path& gameDir) {
    return gameDir / std::string(kAssetsDir);
}

// Get asset indexes directory path
std::filesystem::path get_asset_indexes_dir(const std::filesystem::path& gameDir) {
    return get_assets_dir(gameDir) / std::string(kIndexesDir);
}

// Get asset objects directory path
std::filesystem::path get_asset_objects_dir(const std::filesystem::path& gameDir) {
    return get_assets_dir(gameDir) / std::string(kObjectsDir);
}

// Get client JAR path
std::filesystem::path get_client_jar_path(const std::filesystem::path& gameDir, const std::string& versionId) {
    return get_versions_dir(gameDir) / versionId / (versionId + ".jar");
}

// Get version JSON path
std::filesystem::path get_version_json_path(const std::filesystem::path& gameDir, const std::string& versionId) {
    return get_versions_dir(gameDir) / versionId / (versionId + ".json");
}

// Get asset index JSON path
std::filesystem::path get_asset_index_path(const std::filesystem::path& gameDir, const std::string& assetIndexId) {
    return get_asset_indexes_dir(gameDir) / (assetIndexId + ".json");
}

// Check if a file exists and has content
bool file_exists_and_valid(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec) && 
           std::filesystem::is_regular_file(path, ec) &&
           std::filesystem::file_size(path, ec) > 0;
}

// Format memory size (e.g., "2G", "512M")
std::string format_memory_size(int megabytes) {
    if (megabytes >= 1024 && megabytes % 1024 == 0) {
        return std::to_string(megabytes / 1024) + "G";
    }
    return std::to_string(megabytes) + "M";
}

// Parse memory profile string to megabytes
int parse_memory_profile(const std::string& profile) {
    if (profile.empty()) {
        return 2048; // Default 2GB
    }
    
    std::string numStr;
    char unit = 'M';
    for (char c : profile) {
        if (std::isdigit(static_cast<unsigned char>(c))) {
            numStr += c;
        } else if (std::isalpha(static_cast<unsigned char>(c))) {
            unit = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            break;
        }
    }
    
    if (numStr.empty()) {
        return 2048;
    }
    
    int value = std::stoi(numStr);
    if (unit == 'G') {
        return value * 1024;
    }
    return value;
}

// Get required Java version for Minecraft version
int get_required_java_version(const std::string& mcVersion) {
    // Parse major version from version string (e.g., "1.20.4" -> 20)
    if (mcVersion.empty()) {
        return 17;
    }
    
    // Handle snapshot and special versions
    if (mcVersion.find("w") != std::string::npos || 
        mcVersion.find("-pre") != std::string::npos ||
        mcVersion.find("-rc") != std::string::npos) {
        return 17; // Assume modern for snapshots
    }
    
    // Parse version components
    std::vector<int> parts;
    std::string current;
    for (char c : mcVersion) {
        if (c == '.') {
            if (!current.empty()) {
                parts.push_back(std::stoi(current));
                current.clear();
            }
        } else if (std::isdigit(static_cast<unsigned char>(c))) {
            current += c;
        } else {
            break; // Stop at non-digit, non-dot
        }
    }
    if (!current.empty()) {
        parts.push_back(std::stoi(current));
    }
    
    if (parts.size() < 2) {
        return 17;
    }
    
    int major = parts[0];
    int minor = parts[1];
    
    // Java version requirements based on Minecraft version
    if (major == 1) {
        if (minor >= 21) {
            return 21; // Minecraft 1.21+ requires Java 21
        } else if (minor >= 18) {
            return 17; // Minecraft 1.18-1.20 requires Java 17
        } else if (minor >= 17) {
            return 16; // Minecraft 1.17 requires Java 16
        } else if (minor >= 13) {
            return 8;  // Minecraft 1.13-1.16 requires Java 8
        } else {
            return 8;  // Older versions require Java 8
        }
    }
    
    return 17; // Default to Java 17 for unknown versions
}

// Check if Java runtime is compatible
bool check_java_compatibility(const JavaRuntimeInfo& runtime, int requiredVersion) {
    return runtime.majorVersion >= requiredVersion;
}

// Get the main class based on loader type
std::string get_main_class(const MinecraftVersionPackage& package, LoaderType loaderType) {
    // If package has a main class defined, use it
    if (!package.mainClass.empty()) {
        return package.mainClass;
    }
    
    // Default main classes for different loaders
    switch (loaderType) {
    case LoaderType::Fabric:
        return "net.fabricmc.loader.impl.launch.knot.KnotClient";
    case LoaderType::Quilt:
        return "org.quiltmc.loader.impl.launch.knot.KnotClient";
    case LoaderType::Forge:
    case LoaderType::NeoForge:
        // Forge uses a wrapper that reads from version json
        return "cpw.mods.bootstraplauncher.BootstrapLauncher";
    case LoaderType::OptiFine:
        return "net.minecraft.launchwrapper.Launch";
    case LoaderType::None:
    default:
        return "net.minecraft.client.main.Main";
    }
}

// Build the effective version ID (handles inherited versions)
std::string get_effective_version_id(const MinecraftVersionPackage& package) {
    if (!package.inheritsFrom.empty()) {
        return package.inheritsFrom;
    }
    return package.versionId;
}

// Check if version uses modern argument format (1.13+)
bool uses_modern_argument_format(const MinecraftVersionPackage& package) {
    // Modern format has jvmArguments and gameArguments instead of minecraftArguments
    return !package.jvmArguments.empty() || !package.gameArguments.empty();
}

// Replace template placeholders in arguments
std::string replace_argument_placeholders(const std::string& arg, 
                                          const std::map<std::string, std::string>& placeholders) {
    std::string result = arg;
    for (const auto& [key, value] : placeholders) {
        std::string placeholder = "${" + key + "}";
        size_t pos = 0;
        while ((pos = result.find(placeholder, pos)) != std::string::npos) {
            result.replace(pos, placeholder.length(), value);
            pos += value.length();
        }
    }
    return result;
}

// Get default JVM arguments for memory settings
std::vector<std::string> get_default_jvm_memory_args(int minMemoryMb, int maxMemoryMb) {
    std::vector<std::string> args;
    args.push_back("-Xms" + format_memory_size(minMemoryMb));
    args.push_back("-Xmx" + format_memory_size(maxMemoryMb));
    return args;
}

// Get default GC settings based on memory
std::vector<std::string> get_default_gc_args(int memoryMb) {
    std::vector<std::string> args;
    
    // Use G1GC for larger heaps
    if (memoryMb >= 4096) {
        args.push_back("-XX:+UseG1GC");
        args.push_back("-XX:+ParallelRefProcEnabled");
        args.push_back("-XX:MaxGCPauseMillis=200");
        args.push_back("-XX:+UnlockExperimentalVMOptions");
        args.push_back("-XX:+DisableExplicitGC");
    } else {
        // Use ParallelGC for smaller heaps
        args.push_back("-XX:+UseParallelGC");
        args.push_back("-XX:+ParallelRefProcEnabled");
    }
    
    return args;
}

// Get loader-specific JVM arguments
std::vector<std::string> get_loader_jvm_args(LoaderType loaderType) {
    std::vector<std::string> args;
    
    switch (loaderType) {
    case LoaderType::Forge:
    case LoaderType::NeoForge:
        // Forge needs additional module flags on newer Java versions
        args.push_back("--add-modules");
        args.push_back("ALL-MODULE-PATH");
        args.push_back("--add-opens");
        args.push_back("java.base/java.util.jar=cpw.mods.securejarhandler");
        args.push_back("--add-opens");
        args.push_back("java.base/java.lang.invoke=cpw.mods.securejarhandler");
        args.push_back("--add-exports");
        args.push_back("java.base/sun.security.util=cpw.mods.securejarhandler");
        break;
    case LoaderType::Fabric:
    case LoaderType::Quilt:
        // Fabric/Quilt generally don't need special JVM args
        break;
    case LoaderType::OptiFine:
        // OptiFine with launchwrapper
        break;
    default:
        break;
    }
    
    return args;
}

// Generate a random UUID for offline mode
std::string generate_offline_uuid(const std::string& username) {
    // Simple UUID generation for offline mode
    // In production, this should use a proper UUID v3/v5 based on username
    std::hash<std::string> hasher;
    auto hash = hasher(username);
    
    std::stringstream ss;
    ss << std::hex << std::setfill('0');
    ss << std::setw(8) << (hash & 0xFFFFFFFF) << "-";
    ss << std::setw(4) << ((hash >> 32) & 0xFFFF) << "-";
    ss << std::setw(4) << ((hash >> 48) & 0xFFFF) << "-";
    // Use additional hash for remaining parts to avoid shift overflow
    auto hash2 = hasher(username + "salt");
    ss << std::setw(4) << (hash2 & 0xFFFF) << "-";
    ss << std::setw(12) << (hash2 >> 16);
    
    return ss.str();
}

// Convert UUID to standard format (with dashes)
std::string normalize_uuid(const std::string& uuid) {
    if (uuid.length() == 32) {
        // Add dashes to raw UUID
        return uuid.substr(0, 8) + "-" + 
               uuid.substr(8, 4) + "-" + 
               uuid.substr(12, 4) + "-" + 
               uuid.substr(16, 4) + "-" + 
               uuid.substr(20);
    }
    return uuid;
}

} // anonymous namespace

PreflightResult DefaultLauncherRuntime::preflight(const LaunchRequest& request) const {
    PreflightResult result;
    const auto& manifest = request.manifest;
    
    // Basic manifest validation
    if (manifest.id.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_instance_id", 
            "instance id is empty", "create or load an instance manifest"});
    }
    if (manifest.name.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_name", 
            "instance name is empty", "name the instance before launch"});
    }
    if (manifest.mcVersion.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_mc_version", 
            "minecraft version is empty", "select a minecraft version"});
    }
    if (manifest.gameDir.empty()) {
        result.issues.push_back({PreflightSeverity::Error, "missing_game_dir", 
            "game directory is empty", "set a valid instance directory"});
    }
    
    // Early return if basic validation fails
    if (!result.issues.empty()) {
        result.ready = false;
        return result;
    }
    
    std::filesystem::path gameDir = manifest.gameDir;
    std::filesystem::path versionsDir = get_versions_dir(gameDir);
    std::filesystem::path librariesDir = get_libraries_dir(gameDir);
    
    // 1. Check Java runtime compatibility
    if (manifest.javaProfileId.empty()) {
        result.issues.push_back({PreflightSeverity::Warning, "missing_java_profile", 
            "java profile is not configured", "pick a java runtime profile"});
    } else {
        JavaService javaService;
        auto runtimes = javaService.discover_runtimes();
        
        bool javaFound = false;
        bool javaCompatible = false;
        int requiredJavaVersion = get_required_java_version(manifest.mcVersion);
        
        for (const auto& runtime : runtimes) {
            if (runtime.id == manifest.javaProfileId || 
                runtime.executable.string().find(manifest.javaProfileId) != std::string::npos) {
                javaFound = true;
                javaCompatible = check_java_compatibility(runtime, requiredJavaVersion);
                break;
            }
        }
        
        if (!javaFound) {
            result.issues.push_back({PreflightSeverity::Error, "java_not_found", 
                "configured java runtime not found", 
                "install java " + std::to_string(requiredJavaVersion) + " or select a different runtime"});
        } else if (!javaCompatible) {
            result.issues.push_back({PreflightSeverity::Error, "java_incompatible", 
                "java version is incompatible with minecraft " + manifest.mcVersion, 
                "use java " + std::to_string(requiredJavaVersion) + " or higher"});
        }
    }
    
    // 2. Check Minecraft client JAR
    std::string versionId = manifest.mcVersion;
    std::filesystem::path clientJar = get_client_jar_path(gameDir, versionId);
    
    if (!file_exists_and_valid(clientJar)) {
        // Try to find the actual version JSON to determine the correct client JAR
        std::filesystem::path versionJson = get_version_json_path(gameDir, versionId);
        
        if (!file_exists_and_valid(versionJson)) {
            result.issues.push_back({PreflightSeverity::Error, "missing_version_json", 
                "version json not found: " + versionJson.string(), 
                "download minecraft " + versionId});
        } else {
            result.issues.push_back({PreflightSeverity::Error, "missing_client_jar", 
                "client jar not found: " + clientJar.string(), 
                "download minecraft client for " + versionId});
        }
    }
    
    // 3. Check version JSON and parse libraries
    std::filesystem::path versionJson = get_version_json_path(gameDir, versionId);
    if (file_exists_and_valid(versionJson)) {
        MinecraftVersionPackage package;
        std::string parseError;
        
        if (parse_version_package_file(versionJson, &package, &parseError)) {
            // Check libraries
            LibraryResolver resolver;
            auto libResult = resolver.resolve_libraries(package);
            
            int missingLibraries = 0;
            int corruptedLibraries = 0;
            
            for (const auto& lib : libResult.libraries) {
                auto libPath = librariesDir / lib.localPath;
                if (!std::filesystem::exists(libPath)) {
                    missingLibraries++;
                } else if (!lib.sha1.empty() && !resolver.check_library_integrity(lib, librariesDir)) {
                    corruptedLibraries++;
                }
            }
            
            if (missingLibraries > 0) {
                result.issues.push_back({PreflightSeverity::Error, "missing_libraries", 
                    std::to_string(missingLibraries) + " libraries are missing", 
                    "run library download to fix"});
            }
            if (corruptedLibraries > 0) {
                result.issues.push_back({PreflightSeverity::Error, "corrupted_libraries", 
                    std::to_string(corruptedLibraries) + " libraries have checksum mismatch", 
                    "re-download corrupted libraries"});
            }
            
            // 4. Check native libraries
            std::filesystem::path nativesDir = get_natives_dir(gameDir, versionId);
            int missingNatives = 0;
            
            for (const auto& native : libResult.nativeLibraries) {
                auto nativePath = librariesDir / native.localPath;
                if (!std::filesystem::exists(nativePath)) {
                    missingNatives++;
                }
            }
            
            if (missingNatives > 0) {
                result.issues.push_back({PreflightSeverity::Error, "missing_natives", 
                    std::to_string(missingNatives) + " native libraries are missing", 
                    "download and extract native libraries"});
            }
            
            // Check if natives directory exists and has content
            if (!std::filesystem::exists(nativesDir)) {
                result.issues.push_back({PreflightSeverity::Warning, "natives_not_extracted", 
                    "native libraries not extracted", 
                    "extract native libraries before launch"});
            }
            
            // 5. Check assets index
            std::string assetIndexId = package.assetIndex.id.empty() ? package.assets : package.assetIndex.id;
            if (assetIndexId.empty()) {
                assetIndexId = "legacy"; // Default for very old versions
            }
            
            std::filesystem::path assetIndexPath = get_asset_index_path(gameDir, assetIndexId);
            if (!file_exists_and_valid(assetIndexPath)) {
                result.issues.push_back({PreflightSeverity::Warning, "missing_asset_index", 
                    "asset index not found: " + assetIndexId, 
                    "download asset index"});
            } else {
                // Check if assets directory exists
                std::filesystem::path assetsDir = get_assets_dir(gameDir);
                std::filesystem::path objectsDir = get_asset_objects_dir(gameDir);
                
                if (!std::filesystem::exists(objectsDir)) {
                    result.issues.push_back({PreflightSeverity::Warning, "missing_assets", 
                        "game assets not downloaded", 
                        "download game assets"});
                }
            }
            
            // 6. Check Loader installation status
            if (manifest.loaderType != LoaderType::None) {
                if (manifest.loaderVersion.empty()) {
                    std::string loaderTypeStr = std::string(to_string(manifest.loaderType));
                    result.issues.push_back({PreflightSeverity::Warning, "missing_loader_version", 
                        loaderTypeStr + " version is not specified", 
                        "select a " + loaderTypeStr + " version"});
                }
                
                // Check for loader-specific files
                if (manifest.loaderType == LoaderType::Fabric || manifest.loaderType == LoaderType::Quilt) {
                    // Check for fabric/quilt loader jar in libraries
                    std::string loaderName = (manifest.loaderType == LoaderType::Fabric) ? "fabric" : "quilt";
                    bool loaderFound = false;
                    
                    for (const auto& lib : libResult.libraries) {
                        if (lib.source.name.find(loaderName) != std::string::npos) {
                            auto libPath = librariesDir / lib.localPath;
                            if (std::filesystem::exists(libPath)) {
                                loaderFound = true;
                                break;
                            }
                        }
                    }
                    
                    if (!loaderFound) {
                        result.issues.push_back({PreflightSeverity::Error, "loader_not_installed", 
                            loaderName + " loader libraries not found", 
                            "install " + loaderName + " loader"});
                    }
                } else if (manifest.loaderType == LoaderType::Forge || manifest.loaderType == LoaderType::NeoForge) {
                    // Check for Forge libraries
                    bool forgeFound = false;
                    for (const auto& lib : libResult.libraries) {
                        if (lib.source.name.find("forge") != std::string::npos ||
                            lib.source.name.find("fml") != std::string::npos) {
                            auto libPath = librariesDir / lib.localPath;
                            if (std::filesystem::exists(libPath)) {
                                forgeFound = true;
                                break;
                            }
                        }
                    }
                    
                    if (!forgeFound) {
                        result.issues.push_back({PreflightSeverity::Error, "loader_not_installed", 
                            "forge/neoforge libraries not found", 
                            "install forge/neoforge loader"});
                    }
                }
            }
            
            // OptiFine specific check
            if (manifest.loaderType == LoaderType::OptiFine && manifest.optifineVersion.empty()) {
                result.issues.push_back({PreflightSeverity::Warning, "missing_optifine_version", 
                    "OptiFine version is not pinned", 
                    "record the exact OptiFine build for rollback"});
            }
        } else {
            result.issues.push_back({PreflightSeverity::Error, "invalid_version_json", 
                "failed to parse version json: " + parseError, 
                "re-download version metadata"});
        }
    }
    
    // 7. Check disk space
    std::error_code ec;
    auto space = std::filesystem::space(gameDir, ec);
    if (!ec && space.available < 500 * 1024 * 1024) { // Less than 500MB
        result.issues.push_back({PreflightSeverity::Warning, "low_disk_space", 
            "low disk space available (" + std::to_string(space.available / (1024 * 1024)) + " MB)", 
            "free up disk space to avoid issues"});
    }
    
    // Add recommendations
    if (manifest.loaderType == LoaderType::None) {
        result.recommendations.push_back("vanilla instance detected; no loader bootstrap is required");
    }
    
    if (manifest.memoryProfile.empty()) {
        result.recommendations.push_back("memory profile not set; using default 2GB");
    }
    
    // Determine if ready (no errors)
    result.ready = std::none_of(result.issues.begin(), result.issues.end(), 
        [](const PreflightIssue& issue) {
            return issue.severity == PreflightSeverity::Error;
        });
    
    return result;
}

LaunchCommand DefaultLauncherRuntime::buildCommand(const LaunchRequest& request) const {
    LaunchCommand command;
    const auto& manifest = request.manifest;
    
    // Set working directory
    command.workingDirectory = manifest.gameDir.empty()
        ? std::filesystem::current_path()
        : std::filesystem::path(manifest.gameDir);
    
    // Set executable (Java path)
    // TODO: Look up actual Java executable from javaProfileId
    command.executable = "java";
    
    // Requires console for game output
    command.requiresConsole = true;
    
    // Set environment variables
    command.environment.emplace("DAWN_INSTANCE_ID", request.instanceId);
    command.environment.emplace("DAWN_MINECRAFT_VERSION", manifest.mcVersion);
    
    // Parse memory settings
    int minMemoryMb = 512;
    int maxMemoryMb = 2048; // Default 2GB
    
    if (!manifest.memoryProfile.empty()) {
        maxMemoryMb = parse_memory_profile(manifest.memoryProfile);
        minMemoryMb = std::min(512, maxMemoryMb / 4);
    }
    
    // Build arguments vector
    std::vector<std::string> args;
    
    // 1. JVM Memory Arguments
    auto memoryArgs = get_default_jvm_memory_args(minMemoryMb, maxMemoryMb);
    args.insert(args.end(), memoryArgs.begin(), memoryArgs.end());
    
    // 2. JVM GC Arguments
    auto gcArgs = get_default_gc_args(maxMemoryMb);
    args.insert(args.end(), gcArgs.begin(), gcArgs.end());
    
    // 3. System Properties
    std::filesystem::path gameDir = command.workingDirectory;
    std::filesystem::path nativesDir = get_natives_dir(gameDir, manifest.mcVersion);
    std::filesystem::path librariesDir = get_libraries_dir(gameDir);
    std::filesystem::path assetsDir = get_assets_dir(gameDir);
    
    args.push_back("-Djava.library.path=" + nativesDir.string());
    args.push_back("-Djna.tmpdir=" + nativesDir.string());
    args.push_back("-Dorg.lwjgl.system.SharedLibraryExtractPath=" + nativesDir.string());
    args.push_back("-Dio.netty.native.workdir=" + nativesDir.string());
    args.push_back("-Duser.home=" + std::filesystem::path(getenv("USERPROFILE") ? getenv("USERPROFILE") : getenv("HOME")).string());
    args.push_back("-Djava.awt.headless=false");
    args.push_back("-Dfile.encoding=UTF-8");
    
    // 4. Load version package to get additional JVM arguments
    std::filesystem::path versionJson = get_version_json_path(gameDir, manifest.mcVersion);
    MinecraftVersionPackage package;
    std::string versionError;
    
    bool hasVersionPackage = file_exists_and_valid(versionJson) && 
                             parse_version_package_file(versionJson, &package, &versionError);
    
    if (hasVersionPackage) {
        // Add logging configuration if available
        if (package.logging.has_value() && package.logging->file.has_value()) {
            std::filesystem::path logConfigPath = assetsDir / "log_configs" / package.logging->file->id;
            if (std::filesystem::exists(logConfigPath)) {
                std::string logArg = package.logging->argument;
                // Replace ${path} placeholder
                size_t pos = logArg.find("${path}");
                if (pos != std::string::npos) {
                    logArg.replace(pos, 7, logConfigPath.string());
                }
                args.push_back(logArg);
            }
        }
        
        // Add modern JVM arguments from version package
        if (uses_modern_argument_format(package)) {
            for (const auto& jvmArg : package.jvmArguments) {
                // Filter out classpath argument (we'll build our own)
                if (jvmArg.find("-cp") == std::string::npos && 
                    jvmArg.find("-classpath") == std::string::npos &&
                    jvmArg.find("${classpath}") == std::string::npos) {
                    args.push_back(jvmArg);
                }
            }
        }
    }
    
    // 5. Loader-specific JVM arguments
    auto loaderJvmArgs = get_loader_jvm_args(manifest.loaderType);
    args.insert(args.end(), loaderJvmArgs.begin(), loaderJvmArgs.end());
    
    // 6. Build Classpath
    LibraryResolver resolver;
    std::string classpath;
    std::filesystem::path clientJar = get_client_jar_path(gameDir, manifest.mcVersion);
    
    if (hasVersionPackage) {
        auto libResult = resolver.resolve_libraries(package);
        classpath = resolver.build_classpath(libResult.libraries, librariesDir, clientJar);
    } else {
        // Fallback: just use client jar
        classpath = clientJar.string();
    }
    
    args.push_back("-cp");
    args.push_back(classpath);
    
    // 7. Main Class
    std::string mainClass = hasVersionPackage ? get_main_class(package, manifest.loaderType) 
                                              : "net.minecraft.client.main.Main";
    args.push_back(mainClass);
    
    // 8. Game Arguments
    // Get account information (placeholder - should be retrieved from AccountService)
    std::string username = "Player";
    std::string uuid = generate_offline_uuid(username);
    std::string accessToken = "0"; // Offline mode token
    std::string userType = "legacy";
    
    // TODO: Get actual account info from request or AccountService
    // This would typically be passed in the LaunchRequest or looked up
    
    if (hasVersionPackage && uses_modern_argument_format(package)) {
        // Modern argument format (1.13+)
        std::map<std::string, std::string> placeholders = {
            {"auth_player_name", username},
            {"version_name", manifest.mcVersion},
            {"game_directory", gameDir.string()},
            {"assets_root", assetsDir.string()},
            {"assets_index_name", package.assetIndex.id.empty() ? package.assets : package.assetIndex.id},
            {"auth_uuid", uuid},
            {"auth_access_token", accessToken},
            {"user_type", userType},
            {"version_type", package.type},
            {"resolution_width", "854"},
            {"resolution_height", "480"}
        };
        
        for (const auto& gameArg : package.gameArguments) {
            args.push_back(replace_argument_placeholders(gameArg, placeholders));
        }
    } else if (hasVersionPackage && package.minecraftArguments.has_value()) {
        // Legacy argument format (pre-1.13)
        std::string mcArgs = package.minecraftArguments.value();
        
        // Replace placeholders
        std::map<std::string, std::string> replacements = {
            {"${auth_player_name}", username},
            {"${version_name}", manifest.mcVersion},
            {"${game_directory}", gameDir.string()},
            {"${assets_root}", assetsDir.string()},
            {"${assets_index_name}", package.assets},
            {"${auth_uuid}", uuid},
            {"${auth_access_token}", accessToken},
            {"${user_type}", userType},
            {"${version_type}", package.type}
        };
        
        for (const auto& [placeholder, value] : replacements) {
            size_t pos = 0;
            while ((pos = mcArgs.find(placeholder, pos)) != std::string::npos) {
                mcArgs.replace(pos, placeholder.length(), value);
                pos += value.length();
            }
        }
        
        // Split into individual arguments
        std::istringstream iss(mcArgs);
        std::string arg;
        while (iss >> arg) {
            args.push_back(arg);
        }
    } else {
        // Fallback: build minimal game arguments
        args.push_back("--username");
        args.push_back(username);
        args.push_back("--version");
        args.push_back(manifest.mcVersion);
        args.push_back("--gameDir");
        args.push_back(gameDir.string());
        args.push_back("--assetsDir");
        args.push_back(assetsDir.string());
        args.push_back("--assetIndex");
        args.push_back(hasVersionPackage ? package.assets : "legacy");
        args.push_back("--uuid");
        args.push_back(uuid);
        args.push_back("--accessToken");
        args.push_back(accessToken);
        args.push_back("--userType");
        args.push_back(userType);
        args.push_back("--versionType");
        args.push_back("release");
    }
    
    // 9. Loader-specific game arguments
    if (manifest.loaderType == LoaderType::Fabric || manifest.loaderType == LoaderType::Quilt) {
        // Fabric/Quilt don't need additional game args
    } else if (manifest.loaderType == LoaderType::Forge || manifest.loaderType == LoaderType::NeoForge) {
        // Forge might need additional arguments - these are typically in the version JSON
    }
    
    command.arguments = std::move(args);
    
    return command;
}

} // namespace dawn::core
