#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace dawn::infra::archive {

/// @brief ZIP 压缩进度回调
/// @param currentFile 当前处理的文件
/// @param processedBytes 已处理的字节数
/// @param totalBytes 总字节数
/// @return 返回 false 取消操作
using ZipProgressCallback = std::function<bool(const std::filesystem::path& currentFile, 
                                                std::uint64_t processedBytes, 
                                                std::uint64_t totalBytes)>;

/// @brief 压缩文件或目录到 ZIP 文件
/// @param sourcePaths 要压缩的文件或目录路径列表
/// @param baseDir 基础目录（用于计算 ZIP 内的相对路径）
/// @param zipPath 输出的 ZIP 文件路径
/// @param callback 进度回调（可选）
/// @param error 错误信息输出
/// @return 是否成功
bool create_zip_archive(
    const std::vector<std::filesystem::path>& sourcePaths,
    const std::filesystem::path& baseDir,
    const std::filesystem::path& zipPath,
    ZipProgressCallback callback = nullptr,
    std::string* error = nullptr);

/// @brief 压缩单个目录到 ZIP 文件
/// @param sourceDir 要压缩的目录
/// @param zipPath 输出的 ZIP 文件路径
/// @param callback 进度回调（可选）
/// @param error 错误信息输出
/// @return 是否成功
bool create_zip_archive(
    const std::filesystem::path& sourceDir,
    const std::filesystem::path& zipPath,
    ZipProgressCallback callback = nullptr,
    std::string* error = nullptr);

/// @brief 解压 ZIP 文件
/// @param zipPath ZIP 文件路径
/// @param targetDir 目标目录
/// @param callback 进度回调（可选）
/// @param error 错误信息输出
/// @return 是否成功
bool extract_zip_archive(
    const std::filesystem::path& zipPath,
    const std::filesystem::path& targetDir,
    ZipProgressCallback callback = nullptr,
    std::string* error = nullptr);

/// @brief 获取 ZIP 文件中的文件列表
/// @param zipPath ZIP 文件路径
/// @param error 错误信息输出
/// @return ZIP 中的文件列表
std::vector<std::string> list_zip_contents(
    const std::filesystem::path& zipPath,
    std::string* error = nullptr);

/// @brief 验证 ZIP 文件完整性
/// @param zipPath ZIP 文件路径
/// @param error 错误信息输出
/// @return 是否通过验证
bool verify_zip_archive(
    const std::filesystem::path& zipPath,
    std::string* error = nullptr);

/// @brief 获取 ZIP 文件信息
/// @param zipPath ZIP 文件路径
/// @param totalFiles 输出：文件总数
/// @param totalSize 输出：解压后总大小
/// @param compressedSize 输出：压缩后大小
/// @param error 错误信息输出
/// @return 是否成功获取信息
bool get_zip_info(
    const std::filesystem::path& zipPath,
    std::size_t& totalFiles,
    std::uint64_t& totalSize,
    std::uint64_t& compressedSize,
    std::string* error = nullptr);

} // namespace dawn::infra::archive
