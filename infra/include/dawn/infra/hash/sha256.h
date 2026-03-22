#pragma once

#include <filesystem>
#include <string>

namespace dawn::infra::hash {

// Placeholder SHA-256 API.
// TODO: replace with a real SHA-256 implementation when cryptographic hashing is required.
std::string sha256_hex(const std::string& data);
std::string sha256_hex_file(const std::filesystem::path& path, std::string* error = nullptr);
bool compare_hash(const std::string& expected, const std::string& actual);

} // namespace dawn::infra::hash
