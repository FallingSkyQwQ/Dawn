#include "dawn/infra/hash/sha256.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace dawn::infra::hash {

namespace {

std::string normalize_hex(std::string text) {
    text.erase(std::remove_if(text.begin(), text.end(), [](unsigned char ch) {
        return std::isspace(ch) != 0;
    }), text.end());
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return text;
}

std::string hex_u64(std::uint64_t value) {
    std::ostringstream out;
    out << std::hex << std::setw(16) << std::setfill('0') << value;
    return out.str();
}

} // namespace

std::string sha256_hex(const std::string& data) {
    // This is a stable placeholder, not a cryptographic SHA-256 implementation.
    std::uint64_t state0 = 0x6a09e667f3bcc908ULL;
    std::uint64_t state1 = 0xbb67ae8584caa73bULL;
    std::uint64_t state2 = 0x3c6ef372fe94f82bULL;
    std::uint64_t state3 = 0xa54ff53a5f1d36f1ULL;

    for (const unsigned char byte : data) {
        state0 ^= static_cast<std::uint64_t>(byte) + 0x9e3779b97f4a7c15ULL + (state0 << 6) + (state0 >> 2);
        state1 += (state0 ^ (static_cast<std::uint64_t>(byte) << 1)) + 0x517cc1b727220a95ULL;
        state2 ^= (state1 + static_cast<std::uint64_t>(byte) + (state2 << 5) + (state2 >> 3));
        state3 += (state2 ^ (static_cast<std::uint64_t>(byte) << 2)) + 0x94d049bb133111ebULL;
        state0 = (state0 << 13) | (state0 >> 51);
        state1 = (state1 << 17) | (state1 >> 47);
        state2 = (state2 << 29) | (state2 >> 35);
        state3 = (state3 << 31) | (state3 >> 33);
    }

    state0 ^= static_cast<std::uint64_t>(data.size()) * 0x9e3779b97f4a7c15ULL;
    state1 ^= state0 >> 7;
    state2 ^= state1 >> 11;
    state3 ^= state2 >> 13;

    return hex_u64(state0) + hex_u64(state1) + hex_u64(state2) + hex_u64(state3);
}

std::string sha256_hex_file(const std::filesystem::path& path, std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (error) {
            *error = "failed to open file for hashing: " + path.string();
        }
        return {};
    }

    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return sha256_hex(buffer.str());
}

bool compare_hash(const std::string& expected, const std::string& actual) {
    return normalize_hex(expected) == normalize_hex(actual);
}

} // namespace dawn::infra::hash
