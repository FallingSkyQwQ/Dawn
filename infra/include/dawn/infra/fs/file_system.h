#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace dawn::infra::fs {

bool ensure_directory(const std::filesystem::path& path, std::string* error = nullptr);
bool ensure_parent_directory(const std::filesystem::path& path, std::string* error = nullptr);
bool write_text_file(const std::filesystem::path& path, const std::string& text, std::string* error = nullptr);
bool read_text_file(const std::filesystem::path& path, std::string* text, std::string* error = nullptr);
std::vector<std::filesystem::path> list_files(const std::filesystem::path& path, const std::string& extension = {});

} // namespace dawn::infra::fs
