#pragma once

#include <filesystem>
#include <string>
#include <string_view>

namespace dawn::infra::hash {

std::string sha256_hex(std::string_view data);
std::string sha256_file_hex(const std::filesystem::path& path, std::string* error = nullptr);
inline std::string sha256_hex_file(const std::filesystem::path& path, std::string* error = nullptr) {
    return sha256_file_hex(path, error);
}
bool compare_hash(std::string_view expected, std::string_view actual);

} // namespace dawn::infra::hash
