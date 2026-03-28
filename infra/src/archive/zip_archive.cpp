#include "dawn/infra/archive/zip_archive.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <vector>

#ifdef _WIN32
    #include <windows.h>
    #include <shlobj.h>
    #include <shlwapi.h>
    #include <objbase.h>
    #include <comdef.h>
    #pragma comment(lib, "shell32.lib")
    #pragma comment(lib, "shlwapi.lib")
    #pragma comment(lib, "ole32.lib")
    #pragma comment(lib, "oleaut32.lib")
#else
    // For non-Windows platforms, we'd use miniz or libzip
    // This is a simplified implementation
#endif

namespace dawn::infra::archive {

namespace {

// ============================================================================
// Windows Shell API Implementation
// ============================================================================

#ifdef _WIN32

class ComInitializer {
public:
    ComInitializer() {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    }
    ~ComInitializer() {
        CoUninitialize();
    }
};

std::wstring to_wstring(const std::filesystem::path& path) {
    return path.wstring();
}

bool create_zip_file_windows(const std::filesystem::path& zipPath, std::string* error) {
    // Create an empty ZIP file using Windows Shell
    // ZIP file magic number for empty archive
    static const char emptyZip[] = 
        "PK\x05\x06"  // Local file header signature
        "\x00\x00"    // Number of this disk
        "\x00\x00"    // Disk with central directory
        "\x00\x00"    // Number of entries on this disk
        "\x00\x00"    // Total number of entries
        "\x00\x00\x00\x00"  // Size of central directory
        "\x00\x00\x00\x00"  // Offset of start of central directory
        "\x00\x00";   // ZIP file comment length

    std::ofstream file(zipPath, std::ios::binary);
    if (!file) {
        if (error) *error = "Failed to create ZIP file";
        return false;
    }
    file.write(emptyZip, sizeof(emptyZip) - 1);
    return true;
}

bool add_file_to_zip_windows(
    IShellFolder* zipFolder,
    const std::filesystem::path& sourcePath,
    const std::wstring& destName,
    std::string* error) {
    
    HRESULT hr;
    
    // Get the source file's folder
    std::wstring sourceDir = sourcePath.parent_path().wstring();
    std::wstring sourceFile = sourcePath.filename().wstring();
    
    IShellFolder* desktopFolder = nullptr;
    hr = SHGetDesktopFolder(&desktopFolder);
    if (FAILED(hr)) {
        if (error) *error = "Failed to get desktop folder";
        return false;
    }
    
    // Parse the source file path
    LPITEMIDLIST sourcePidl = nullptr;
    hr = desktopFolder->ParseDisplayName(nullptr, nullptr, const_cast<LPWSTR>(sourcePath.wstring().c_str()), 
                                          nullptr, &sourcePidl, nullptr);
    desktopFolder->Release();
    
    if (FAILED(hr)) {
        if (error) *error = "Failed to parse source path";
        return false;
    }
    
    // Use SHFileOperation to copy the file into the ZIP
    // This is a simplified approach - in production, use more robust methods
    CoTaskMemFree(sourcePidl);
    
    return true;
}

#endif // _WIN32

// ============================================================================
// Cross-platform fallback using simple implementation
// ============================================================================

// Local file header structure
struct LocalFileHeader {
    uint32_t signature = 0x04034b50;  // 'PK\x03\x04'
    uint16_t version = 20;             // 2.0
    uint16_t flags = 0;
    uint16_t compression = 0;          // 0 = stored, 8 = deflated
    uint16_t modTime = 0;
    uint16_t modDate = 0;
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t nameLength = 0;
    uint16_t extraLength = 0;
};

// Central directory header structure
struct CentralDirectoryHeader {
    uint32_t signature = 0x02014b50;  // 'PK\x01\x02'
    uint16_t versionMadeBy = 20;
    uint16_t versionNeeded = 20;
    uint16_t flags = 0;
    uint16_t compression = 0;
    uint16_t modTime = 0;
    uint16_t modDate = 0;
    uint32_t crc32 = 0;
    uint32_t compressedSize = 0;
    uint32_t uncompressedSize = 0;
    uint16_t nameLength = 0;
    uint16_t extraLength = 0;
    uint16_t commentLength = 0;
    uint16_t diskNumber = 0;
    uint16_t internalAttr = 0;
    uint32_t externalAttr = 0;
    uint32_t localHeaderOffset = 0;
};

// End of central directory record
struct EndOfCentralDirectory {
    uint32_t signature = 0x06054b50;  // 'PK\x05\x06'
    uint16_t diskNumber = 0;
    uint16_t centralDirDisk = 0;
    uint16_t numEntriesOnDisk = 0;
    uint16_t totalEntries = 0;
    uint32_t centralDirSize = 0;
    uint32_t centralDirOffset = 0;
    uint16_t commentLength = 0;
};

// Simple CRC32 calculation
uint32_t crc32_table[256];
bool crc32_initialized = false;

void init_crc32() {
    if (crc32_initialized) return;
    for (int i = 0; i < 256; ++i) {
        uint32_t crc = i;
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
        crc32_table[i] = crc;
    }
    crc32_initialized = true;
}

uint32_t calculate_crc32(const uint8_t* data, size_t length) {
    init_crc32();
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc = (crc >> 8) ^ crc32_table[(crc ^ data[i]) & 0xFF];
    }
    return ~crc;
}

uint32_t calculate_crc32(const std::string& data) {
    return calculate_crc32(reinterpret_cast<const uint8_t*>(data.data()), data.size());
}

// Simple date/time conversion for ZIP format
uint16_t dos_date(const std::filesystem::file_time_type& time) {
    // Simplified - return current date
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    return static_cast<uint16_t>(((tm.tm_year - 80) << 9) | ((tm.tm_mon + 1) << 5) | tm.tm_mday);
}

uint16_t dos_time(const std::filesystem::file_time_type& time) {
    // Simplified - return current time
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    return static_cast<uint16_t>((tm.tm_hour << 11) | (tm.tm_min << 5) | (tm.tm_sec >> 1));
}

// Collect all files recursively
void collect_files_recursive(
    const std::filesystem::path& baseDir,
    const std::filesystem::path& currentPath,
    std::vector<std::filesystem::path>& files,
    std::uint64_t& totalSize) {
    
    std::error_code ec;
    if (std::filesystem::is_directory(currentPath, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(currentPath, ec)) {
            collect_files_recursive(baseDir, entry.path(), files, totalSize);
        }
    } else if (std::filesystem::is_regular_file(currentPath, ec)) {
        files.push_back(currentPath);
        totalSize += std::filesystem::file_size(currentPath, ec);
    }
}

// Write little-endian values
template<typename T>
void write_le(std::ostream& os, T value) {
    for (size_t i = 0; i < sizeof(T); ++i) {
        os.put(static_cast<char>((value >> (i * 8)) & 0xFF));
    }
}

void write_local_header(std::ostream& os, const LocalFileHeader& header) {
    write_le(os, header.signature);
    write_le(os, header.version);
    write_le(os, header.flags);
    write_le(os, header.compression);
    write_le(os, header.modTime);
    write_le(os, header.modDate);
    write_le(os, header.crc32);
    write_le(os, header.compressedSize);
    write_le(os, header.uncompressedSize);
    write_le(os, header.nameLength);
    write_le(os, header.extraLength);
}

void write_central_header(std::ostream& os, const CentralDirectoryHeader& header) {
    write_le(os, header.signature);
    write_le(os, header.versionMadeBy);
    write_le(os, header.versionNeeded);
    write_le(os, header.flags);
    write_le(os, header.compression);
    write_le(os, header.modTime);
    write_le(os, header.modDate);
    write_le(os, header.crc32);
    write_le(os, header.compressedSize);
    write_le(os, header.uncompressedSize);
    write_le(os, header.nameLength);
    write_le(os, header.extraLength);
    write_le(os, header.commentLength);
    write_le(os, header.diskNumber);
    write_le(os, header.internalAttr);
    write_le(os, header.externalAttr);
    write_le(os, header.localHeaderOffset);
}

void write_end_of_central_dir(std::ostream& os, const EndOfCentralDirectory& eocd) {
    write_le(os, eocd.signature);
    write_le(os, eocd.diskNumber);
    write_le(os, eocd.centralDirDisk);
    write_le(os, eocd.numEntriesOnDisk);
    write_le(os, eocd.totalEntries);
    write_le(os, eocd.centralDirSize);
    write_le(os, eocd.centralDirOffset);
    write_le(os, eocd.commentLength);
}

} // anonymous namespace

// ============================================================================
// Public API Implementation
// ============================================================================

bool create_zip_archive(
    const std::vector<std::filesystem::path>& sourcePaths,
    const std::filesystem::path& baseDir,
    const std::filesystem::path& zipPath,
    ZipProgressCallback callback,
    std::string* error) {
    
    std::error_code ec;
    
    // Ensure parent directory exists
    std::filesystem::create_directories(zipPath.parent_path(), ec);
    if (ec) {
        if (error) *error = "Failed to create parent directory: " + ec.message();
        return false;
    }
    
    // Collect all files
    std::vector<std::filesystem::path> allFiles;
    std::uint64_t totalSize = 0;
    
    for (const auto& sourcePath : sourcePaths) {
        if (!std::filesystem::exists(sourcePath, ec)) {
            continue;
        }
        collect_files_recursive(baseDir, sourcePath, allFiles, totalSize);
    }
    
    if (allFiles.empty()) {
        if (error) *error = "No files to archive";
        return false;
    }
    
    // Open output file
    std::ofstream zipFile(zipPath, std::ios::binary);
    if (!zipFile) {
        if (error) *error = "Failed to open ZIP file for writing";
        return false;
    }
    
    // Track central directory entries
    struct CentralDirEntry {
        CentralDirectoryHeader header;
        std::string name;
    };
    std::vector<CentralDirEntry> centralDir;
    std::uint64_t processedBytes = 0;
    
    // Process each file
    for (const auto& filePath : allFiles) {
        // Calculate relative path for ZIP entry name
        std::filesystem::path relativePath;
        std::error_code relEc;
        relativePath = std::filesystem::relative(filePath, baseDir, relEc);
        if (relEc) {
            relativePath = filePath.filename();
        }
        
        std::string entryName = relativePath.generic_string();
        
        // Read file content
        std::ifstream inFile(filePath, std::ios::binary);
        if (!inFile) {
            if (error) *error = "Failed to read file: " + filePath.string();
            return false;
        }
        
        std::string content((std::istreambuf_iterator<char>(inFile)),
                            std::istreambuf_iterator<char>());
        inFile.close();
        
        // Calculate CRC32
        uint32_t crc = calculate_crc32(content);
        
        // Get file modification time
        auto modTime = std::filesystem::last_write_time(filePath, ec);
        
        // Write local file header
        LocalFileHeader localHeader;
        localHeader.crc32 = crc;
        localHeader.compressedSize = static_cast<uint32_t>(content.size());
        localHeader.uncompressedSize = static_cast<uint32_t>(content.size());
        localHeader.nameLength = static_cast<uint16_t>(entryName.size());
        localHeader.modTime = dos_time(modTime);
        localHeader.modDate = dos_date(modTime);
        
        std::streampos localHeaderOffset = zipFile.tellp();
        write_local_header(zipFile, localHeader);
        zipFile.write(entryName.data(), entryName.size());
        zipFile.write(content.data(), content.size());
        
        // Prepare central directory entry
        CentralDirectoryHeader centralHeader;
        centralHeader.crc32 = crc;
        centralHeader.compressedSize = static_cast<uint32_t>(content.size());
        centralHeader.uncompressedSize = static_cast<uint32_t>(content.size());
        centralHeader.nameLength = static_cast<uint16_t>(entryName.size());
        centralHeader.localHeaderOffset = static_cast<uint32_t>(localHeaderOffset);
        centralHeader.modTime = localHeader.modTime;
        centralHeader.modDate = localHeader.modDate;
        centralHeader.externalAttr = std::filesystem::is_directory(filePath, ec) ? 0x10 : 0x20;
        
        centralDir.push_back({centralHeader, entryName});
        
        // Update progress
        processedBytes += content.size();
        if (callback) {
            bool shouldContinue = callback(filePath, processedBytes, totalSize);
            if (!shouldContinue) {
                if (error) *error = "Operation cancelled by user";
                zipFile.close();
                std::filesystem::remove(zipPath, ec);
                return false;
            }
        }
    }
    
    // Write central directory
    std::streampos centralDirOffset = zipFile.tellp();
    for (const auto& entry : centralDir) {
        write_central_header(zipFile, entry.header);
        zipFile.write(entry.name.data(), entry.name.size());
    }
    
    // Write end of central directory
    EndOfCentralDirectory eocd;
    eocd.numEntriesOnDisk = static_cast<uint16_t>(centralDir.size());
    eocd.totalEntries = static_cast<uint16_t>(centralDir.size());
    eocd.centralDirSize = static_cast<uint32_t>(zipFile.tellp() - centralDirOffset);
    eocd.centralDirOffset = static_cast<uint32_t>(centralDirOffset);
    
    write_end_of_central_dir(zipFile, eocd);
    
    zipFile.close();
    return true;
}

bool create_zip_archive(
    const std::filesystem::path& sourceDir,
    const std::filesystem::path& zipPath,
    ZipProgressCallback callback,
    std::string* error) {
    
    return create_zip_archive(std::vector{sourceDir}, sourceDir, zipPath, callback, error);
}

bool extract_zip_archive(
    const std::filesystem::path& zipPath,
    const std::filesystem::path& targetDir,
    ZipProgressCallback callback,
    std::string* error) {
    
    std::error_code ec;
    
    if (!std::filesystem::exists(zipPath, ec)) {
        if (error) *error = "ZIP file does not exist";
        return false;
    }
    
    // Create target directory
    std::filesystem::create_directories(targetDir, ec);
    if (ec) {
        if (error) *error = "Failed to create target directory: " + ec.message();
        return false;
    }
    
    // Open ZIP file
    std::ifstream zipFile(zipPath, std::ios::binary);
    if (!zipFile) {
        if (error) *error = "Failed to open ZIP file";
        return false;
    }
    
    // Read end of central directory
    zipFile.seekg(0, std::ios::end);
    auto fileSize = zipFile.tellg();
    
    if (fileSize < static_cast<std::streamoff>(sizeof(EndOfCentralDirectory))) {
        if (error) *error = "Invalid ZIP file (too small)";
        return false;
    }
    
    // Search for end of central directory signature
    bool found = false;
    EndOfCentralDirectory eocd;
    for (std::streamoff offset = 22; offset <= fileSize; ++offset) {  // 22 is minimum EOCD size
        std::streamoff pos = fileSize - offset;
        if (pos < 0) break;
        zipFile.seekg(pos);
        uint32_t sig;
        zipFile.read(reinterpret_cast<char*>(&sig), sizeof(sig));
        if (sig == 0x06054b50) {
            zipFile.seekg(pos);
            // Read EOCD (skipping comment)
            zipFile.read(reinterpret_cast<char*>(&eocd.signature), sizeof(eocd.signature));
            zipFile.read(reinterpret_cast<char*>(&eocd.diskNumber), sizeof(eocd.diskNumber));
            zipFile.read(reinterpret_cast<char*>(&eocd.centralDirDisk), sizeof(eocd.centralDirDisk));
            zipFile.read(reinterpret_cast<char*>(&eocd.numEntriesOnDisk), sizeof(eocd.numEntriesOnDisk));
            zipFile.read(reinterpret_cast<char*>(&eocd.totalEntries), sizeof(eocd.totalEntries));
            zipFile.read(reinterpret_cast<char*>(&eocd.centralDirSize), sizeof(eocd.centralDirSize));
            zipFile.read(reinterpret_cast<char*>(&eocd.centralDirOffset), sizeof(eocd.centralDirOffset));
            zipFile.read(reinterpret_cast<char*>(&eocd.commentLength), sizeof(eocd.commentLength));
            found = true;
            break;
        }
    }
    
    if (!found) {
        if (error) *error = "Invalid ZIP file (EOCD not found)";
        return false;
    }
    
    // Read central directory and extract files
    zipFile.seekg(eocd.centralDirOffset);
    
    std::uint64_t totalExtracted = 0;
    std::uint64_t totalSize = 0;
    
    // First pass: calculate total size
    auto savedPos = zipFile.tellg();
    for (uint16_t i = 0; i < eocd.totalEntries; ++i) {
        CentralDirectoryHeader header;
        zipFile.read(reinterpret_cast<char*>(&header.signature), sizeof(header.signature));
        zipFile.read(reinterpret_cast<char*>(&header.versionMadeBy), sizeof(header.versionMadeBy));
        zipFile.read(reinterpret_cast<char*>(&header.versionNeeded), sizeof(header.versionNeeded));
        zipFile.read(reinterpret_cast<char*>(&header.flags), sizeof(header.flags));
        zipFile.read(reinterpret_cast<char*>(&header.compression), sizeof(header.compression));
        zipFile.read(reinterpret_cast<char*>(&header.modTime), sizeof(header.modTime));
        zipFile.read(reinterpret_cast<char*>(&header.modDate), sizeof(header.modDate));
        zipFile.read(reinterpret_cast<char*>(&header.crc32), sizeof(header.crc32));
        zipFile.read(reinterpret_cast<char*>(&header.compressedSize), sizeof(header.compressedSize));
        zipFile.read(reinterpret_cast<char*>(&header.uncompressedSize), sizeof(header.uncompressedSize));
        zipFile.read(reinterpret_cast<char*>(&header.nameLength), sizeof(header.nameLength));
        zipFile.read(reinterpret_cast<char*>(&header.extraLength), sizeof(header.extraLength));
        zipFile.read(reinterpret_cast<char*>(&header.commentLength), sizeof(header.commentLength));
        zipFile.read(reinterpret_cast<char*>(&header.diskNumber), sizeof(header.diskNumber));
        zipFile.read(reinterpret_cast<char*>(&header.internalAttr), sizeof(header.internalAttr));
        zipFile.read(reinterpret_cast<char*>(&header.externalAttr), sizeof(header.externalAttr));
        zipFile.read(reinterpret_cast<char*>(&header.localHeaderOffset), sizeof(header.localHeaderOffset));
        
        std::string name(header.nameLength, '\0');
        zipFile.read(name.data(), header.nameLength);
        zipFile.seekg(header.extraLength + header.commentLength, std::ios::cur);
        
        totalSize += header.uncompressedSize;
    }
    zipFile.seekg(savedPos);
    
    // Second pass: extract files
    for (uint16_t i = 0; i < eocd.totalEntries; ++i) {
        CentralDirectoryHeader header;
        zipFile.read(reinterpret_cast<char*>(&header.signature), sizeof(header.signature));
        zipFile.read(reinterpret_cast<char*>(&header.versionMadeBy), sizeof(header.versionMadeBy));
        zipFile.read(reinterpret_cast<char*>(&header.versionNeeded), sizeof(header.versionNeeded));
        zipFile.read(reinterpret_cast<char*>(&header.flags), sizeof(header.flags));
        zipFile.read(reinterpret_cast<char*>(&header.compression), sizeof(header.compression));
        zipFile.read(reinterpret_cast<char*>(&header.modTime), sizeof(header.modTime));
        zipFile.read(reinterpret_cast<char*>(&header.modDate), sizeof(header.modDate));
        zipFile.read(reinterpret_cast<char*>(&header.crc32), sizeof(header.crc32));
        zipFile.read(reinterpret_cast<char*>(&header.compressedSize), sizeof(header.compressedSize));
        zipFile.read(reinterpret_cast<char*>(&header.uncompressedSize), sizeof(header.uncompressedSize));
        zipFile.read(reinterpret_cast<char*>(&header.nameLength), sizeof(header.nameLength));
        zipFile.read(reinterpret_cast<char*>(&header.extraLength), sizeof(header.extraLength));
        zipFile.read(reinterpret_cast<char*>(&header.commentLength), sizeof(header.commentLength));
        zipFile.read(reinterpret_cast<char*>(&header.diskNumber), sizeof(header.diskNumber));
        zipFile.read(reinterpret_cast<char*>(&header.internalAttr), sizeof(header.internalAttr));
        zipFile.read(reinterpret_cast<char*>(&header.externalAttr), sizeof(header.externalAttr));
        zipFile.read(reinterpret_cast<char*>(&header.localHeaderOffset), sizeof(header.localHeaderOffset));
        
        std::string name(header.nameLength, '\0');
        zipFile.read(name.data(), header.nameLength);
        zipFile.seekg(header.extraLength + header.commentLength, std::ios::cur);
        
        // Skip directories
        if (name.back() == '/') {
            std::filesystem::create_directories(targetDir / name, ec);
            continue;
        }
        
        // Read local file header to get to the data
        auto entryPos = zipFile.tellg();
        zipFile.seekg(header.localHeaderOffset);
        
        LocalFileHeader localHeader;
        zipFile.read(reinterpret_cast<char*>(&localHeader.signature), sizeof(localHeader.signature));
        zipFile.read(reinterpret_cast<char*>(&localHeader.version), sizeof(localHeader.version));
        zipFile.read(reinterpret_cast<char*>(&localHeader.flags), sizeof(localHeader.flags));
        zipFile.read(reinterpret_cast<char*>(&localHeader.compression), sizeof(localHeader.compression));
        zipFile.read(reinterpret_cast<char*>(&localHeader.modTime), sizeof(localHeader.modTime));
        zipFile.read(reinterpret_cast<char*>(&localHeader.modDate), sizeof(localHeader.modDate));
        zipFile.read(reinterpret_cast<char*>(&localHeader.crc32), sizeof(localHeader.crc32));
        zipFile.read(reinterpret_cast<char*>(&localHeader.compressedSize), sizeof(localHeader.compressedSize));
        zipFile.read(reinterpret_cast<char*>(&localHeader.uncompressedSize), sizeof(localHeader.uncompressedSize));
        zipFile.read(reinterpret_cast<char*>(&localHeader.nameLength), sizeof(localHeader.nameLength));
        zipFile.read(reinterpret_cast<char*>(&localHeader.extraLength), sizeof(localHeader.extraLength));
        
        zipFile.seekg(localHeader.nameLength + localHeader.extraLength, std::ios::cur);
        
        // Read file data
        std::string data(header.compressedSize, '\0');
        zipFile.read(data.data(), header.compressedSize);
        
        // Create output directory
        auto outPath = targetDir / name;
        std::filesystem::create_directories(outPath.parent_path(), ec);
        
        // Write file
        std::ofstream outFile(outPath, std::ios::binary);
        if (!outFile) {
            if (error) *error = "Failed to create output file: " + outPath.string();
            return false;
        }
        outFile.write(data.data(), data.size());
        outFile.close();
        
        // Restore position for next central directory entry
        zipFile.seekg(entryPos);
        
        // Update progress
        totalExtracted += header.uncompressedSize;
        if (callback) {
            bool shouldContinue = callback(outPath, totalExtracted, totalSize);
            if (!shouldContinue) {
                if (error) *error = "Operation cancelled by user";
                return false;
            }
        }
    }
    
    zipFile.close();
    return true;
}

std::vector<std::string> list_zip_contents(
    const std::filesystem::path& zipPath,
    std::string* error) {
    
    std::vector<std::string> contents;
    std::error_code ec;
    
    if (!std::filesystem::exists(zipPath, ec)) {
        if (error) *error = "ZIP file does not exist";
        return contents;
    }
    
    std::ifstream zipFile(zipPath, std::ios::binary);
    if (!zipFile) {
        if (error) *error = "Failed to open ZIP file";
        return contents;
    }
    
    // Simplified implementation - just return empty list for now
    // Full implementation would parse the central directory
    
    zipFile.close();
    return contents;
}

bool verify_zip_archive(
    const std::filesystem::path& zipPath,
    std::string* error) {
    
    std::error_code ec;
    
    if (!std::filesystem::exists(zipPath, ec)) {
        if (error) *error = "ZIP file does not exist";
        return false;
    }
    
    // Check file size
    auto size = std::filesystem::file_size(zipPath, ec);
    if (ec || size < 22) {  // Minimum ZIP size
        if (error) *error = "ZIP file is too small or inaccessible";
        return false;
    }
    
    // Check ZIP signature
    std::ifstream file(zipPath, std::ios::binary);
    if (!file) {
        if (error) *error = "Failed to open ZIP file";
        return false;
    }
    
    uint32_t signature;
    file.read(reinterpret_cast<char*>(&signature), sizeof(signature));
    file.close();
    
    // ZIP files can start with local file header (0x04034b50) or 
    // span signature (0x08074b50) or empty ZIP (EOCD at start)
    if (signature != 0x04034b50 && signature != 0x08074b50 && 
        signature != 0x06054b50) {
        if (error) *error = "Invalid ZIP file signature";
        return false;
    }
    
    return true;
}

bool get_zip_info(
    const std::filesystem::path& zipPath,
    std::size_t& totalFiles,
    std::uint64_t& totalSize,
    std::uint64_t& compressedSize,
    std::string* error) {
    
    totalFiles = 0;
    totalSize = 0;
    compressedSize = 0;
    
    std::error_code ec;
    
    if (!std::filesystem::exists(zipPath, ec)) {
        if (error) *error = "ZIP file does not exist";
        return false;
    }
    
    compressedSize = std::filesystem::file_size(zipPath, ec);
    
    std::ifstream zipFile(zipPath, std::ios::binary);
    if (!zipFile) {
        if (error) *error = "Failed to open ZIP file";
        return false;
    }
    
    // Find and read EOCD
    zipFile.seekg(0, std::ios::end);
    auto fileSize = zipFile.tellg();
    
    bool found = false;
    EndOfCentralDirectory eocd;
    for (std::streamoff offset = 22; offset <= fileSize; ++offset) {
        std::streamoff pos = fileSize - offset;
        if (pos < 0) break;
        zipFile.seekg(pos);
        uint32_t sig;
        zipFile.read(reinterpret_cast<char*>(&sig), sizeof(sig));
        if (sig == 0x06054b50) {
            zipFile.seekg(pos);
            zipFile.read(reinterpret_cast<char*>(&eocd.signature), sizeof(eocd.signature));
            zipFile.read(reinterpret_cast<char*>(&eocd.diskNumber), sizeof(eocd.diskNumber));
            zipFile.read(reinterpret_cast<char*>(&eocd.centralDirDisk), sizeof(eocd.centralDirDisk));
            zipFile.read(reinterpret_cast<char*>(&eocd.numEntriesOnDisk), sizeof(eocd.numEntriesOnDisk));
            zipFile.read(reinterpret_cast<char*>(&eocd.totalEntries), sizeof(eocd.totalEntries));
            zipFile.read(reinterpret_cast<char*>(&eocd.centralDirSize), sizeof(eocd.centralDirSize));
            zipFile.read(reinterpret_cast<char*>(&eocd.centralDirOffset), sizeof(eocd.centralDirOffset));
            zipFile.read(reinterpret_cast<char*>(&eocd.commentLength), sizeof(eocd.commentLength));
            found = true;
            break;
        }
    }
    
    if (!found) {
        if (error) *error = "Invalid ZIP file format";
        return false;
    }
    
    totalFiles = eocd.totalEntries;
    
    // Read central directory to get total uncompressed size
    zipFile.seekg(eocd.centralDirOffset);
    for (uint16_t i = 0; i < eocd.totalEntries; ++i) {
        CentralDirectoryHeader header;
        zipFile.read(reinterpret_cast<char*>(&header.signature), sizeof(header.signature));
        zipFile.read(reinterpret_cast<char*>(&header.versionMadeBy), sizeof(header.versionMadeBy));
        zipFile.read(reinterpret_cast<char*>(&header.versionNeeded), sizeof(header.versionNeeded));
        zipFile.read(reinterpret_cast<char*>(&header.flags), sizeof(header.flags));
        zipFile.read(reinterpret_cast<char*>(&header.compression), sizeof(header.compression));
        zipFile.read(reinterpret_cast<char*>(&header.modTime), sizeof(header.modTime));
        zipFile.read(reinterpret_cast<char*>(&header.modDate), sizeof(header.modDate));
        zipFile.read(reinterpret_cast<char*>(&header.crc32), sizeof(header.crc32));
        zipFile.read(reinterpret_cast<char*>(&header.compressedSize), sizeof(header.compressedSize));
        zipFile.read(reinterpret_cast<char*>(&header.uncompressedSize), sizeof(header.uncompressedSize));
        zipFile.read(reinterpret_cast<char*>(&header.nameLength), sizeof(header.nameLength));
        zipFile.read(reinterpret_cast<char*>(&header.extraLength), sizeof(header.extraLength));
        zipFile.read(reinterpret_cast<char*>(&header.commentLength), sizeof(header.commentLength));
        zipFile.read(reinterpret_cast<char*>(&header.diskNumber), sizeof(header.diskNumber));
        zipFile.read(reinterpret_cast<char*>(&header.internalAttr), sizeof(header.internalAttr));
        zipFile.read(reinterpret_cast<char*>(&header.externalAttr), sizeof(header.externalAttr));
        zipFile.read(reinterpret_cast<char*>(&header.localHeaderOffset), sizeof(header.localHeaderOffset));
        
        totalSize += header.uncompressedSize;
        
        // Skip variable length fields
        zipFile.seekg(header.nameLength + header.extraLength + header.commentLength, std::ios::cur);
    }
    
    zipFile.close();
    return true;
}

} // namespace dawn::infra::archive
