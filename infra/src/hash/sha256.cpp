#include "dawn/infra/hash/sha256.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <string_view>

namespace dawn::infra::hash {

namespace {

struct Sha256Context {
    std::array<std::uint32_t, 8> state{
        0x6a09e667U,
        0xbb67ae85U,
        0x3c6ef372U,
        0xa54ff53aU,
        0x510e527fU,
        0x9b05688cU,
        0x1f83d9abU,
        0x5be0cd19U,
    };
    std::array<std::uint8_t, 64> buffer{};
    std::uint64_t bitLength = 0;
    std::size_t bufferSize = 0;
};

constexpr std::array<std::uint32_t, 64> kRoundConstants = {
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U, 0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U, 0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU, 0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U, 0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U, 0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U, 0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U, 0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U, 0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

inline std::uint32_t rotr(std::uint32_t value, std::uint32_t bits) {
    return (value >> bits) | (value << (32U - bits));
}

inline std::uint32_t ch(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (~x & z);
}

inline std::uint32_t maj(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    return (x & y) ^ (x & z) ^ (y & z);
}

inline std::uint32_t big_sigma0(std::uint32_t x) {
    return rotr(x, 2U) ^ rotr(x, 13U) ^ rotr(x, 22U);
}

inline std::uint32_t big_sigma1(std::uint32_t x) {
    return rotr(x, 6U) ^ rotr(x, 11U) ^ rotr(x, 25U);
}

inline std::uint32_t small_sigma0(std::uint32_t x) {
    return rotr(x, 7U) ^ rotr(x, 18U) ^ (x >> 3U);
}

inline std::uint32_t small_sigma1(std::uint32_t x) {
    return rotr(x, 17U) ^ rotr(x, 19U) ^ (x >> 10U);
}

inline std::uint32_t read_be32(const std::uint8_t* data) {
    return (static_cast<std::uint32_t>(data[0]) << 24U) |
        (static_cast<std::uint32_t>(data[1]) << 16U) |
        (static_cast<std::uint32_t>(data[2]) << 8U) |
        static_cast<std::uint32_t>(data[3]);
}

inline void write_be32(std::uint32_t value, std::uint8_t* out) {
    out[0] = static_cast<std::uint8_t>((value >> 24U) & 0xffU);
    out[1] = static_cast<std::uint8_t>((value >> 16U) & 0xffU);
    out[2] = static_cast<std::uint8_t>((value >> 8U) & 0xffU);
    out[3] = static_cast<std::uint8_t>(value & 0xffU);
}

void transform(Sha256Context& context, const std::uint8_t block[64]) {
    std::array<std::uint32_t, 64> schedule{};
    for (std::size_t index = 0; index < 16; ++index) {
        schedule[index] = read_be32(block + (index * 4U));
    }
    for (std::size_t index = 16; index < 64; ++index) {
        schedule[index] = small_sigma1(schedule[index - 2]) + schedule[index - 7] + small_sigma0(schedule[index - 15]) + schedule[index - 16];
    }

    std::uint32_t a = context.state[0];
    std::uint32_t b = context.state[1];
    std::uint32_t c = context.state[2];
    std::uint32_t d = context.state[3];
    std::uint32_t e = context.state[4];
    std::uint32_t f = context.state[5];
    std::uint32_t g = context.state[6];
    std::uint32_t h = context.state[7];

    for (std::size_t index = 0; index < 64; ++index) {
        const std::uint32_t temp1 = h + big_sigma1(e) + ch(e, f, g) + kRoundConstants[index] + schedule[index];
        const std::uint32_t temp2 = big_sigma0(a) + maj(a, b, c);
        h = g;
        g = f;
        f = e;
        e = d + temp1;
        d = c;
        c = b;
        b = a;
        a = temp1 + temp2;
    }

    context.state[0] += a;
    context.state[1] += b;
    context.state[2] += c;
    context.state[3] += d;
    context.state[4] += e;
    context.state[5] += f;
    context.state[6] += g;
    context.state[7] += h;
}

void update(Sha256Context& context, const std::uint8_t* data, std::size_t length) {
    context.bitLength += static_cast<std::uint64_t>(length) * 8ULL;

    while (length > 0) {
        const std::size_t space = 64U - context.bufferSize;
        const std::size_t toCopy = std::min(space, length);
        std::copy_n(data, toCopy, context.buffer.begin() + context.bufferSize);
        context.bufferSize += toCopy;
        data += toCopy;
        length -= toCopy;

        if (context.bufferSize == 64U) {
            transform(context, context.buffer.data());
            context.bufferSize = 0;
        }
    }
}

std::array<std::uint8_t, 32> finalize(Sha256Context context) {
    context.buffer[context.bufferSize++] = 0x80U;

    if (context.bufferSize > 56U) {
        while (context.bufferSize < 64U) {
            context.buffer[context.bufferSize++] = 0x00U;
        }
        transform(context, context.buffer.data());
        context.bufferSize = 0;
    }

    while (context.bufferSize < 56U) {
        context.buffer[context.bufferSize++] = 0x00U;
    }

    for (int offset = 7; offset >= 0; --offset) {
        context.buffer[context.bufferSize++] = static_cast<std::uint8_t>((context.bitLength >> (offset * 8)) & 0xffU);
    }
    transform(context, context.buffer.data());

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < context.state.size(); ++index) {
        write_be32(context.state[index], digest.data() + (index * 4U));
    }
    return digest;
}

std::string to_hex(const std::array<std::uint8_t, 32>& digest) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const auto byte : digest) {
        out << std::setw(2) << static_cast<int>(byte);
    }
    return out.str();
}

std::string normalize_hash(std::string_view value) {
    std::string text;
    text.reserve(value.size());
    for (char ch : value) {
        if (!std::isspace(static_cast<unsigned char>(ch))) {
            text.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }

    constexpr std::string_view kPrefix = "sha256:";
    if (text.rfind(kPrefix, 0) == 0) {
        text.erase(0, kPrefix.size());
    }
    return text;
}

} // namespace

std::string sha256_hex(std::string_view data) {
    Sha256Context context;
    update(context, reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
    return to_hex(finalize(context));
}

std::string sha256_file_hex(const std::filesystem::path& path, std::string* error) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream.is_open()) {
        if (error) {
            *error = "failed to open file for hashing: " + path.string();
        }
        return {};
    }

    Sha256Context context;
    std::array<char, 4096> buffer{};
    while (stream.good()) {
        stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const auto read = stream.gcount();
        if (read > 0) {
            update(context, reinterpret_cast<const std::uint8_t*>(buffer.data()), static_cast<std::size_t>(read));
        }
    }

    if (!stream.eof() && stream.fail()) {
        if (error) {
            *error = "failed to read file for hashing: " + path.string();
        }
        return {};
    }

    if (error) {
        error->clear();
    }
    return to_hex(finalize(context));
}

bool compare_hash(std::string_view expected, std::string_view actual) {
    return normalize_hash(expected) == normalize_hash(actual);
}

} // namespace dawn::infra::hash
