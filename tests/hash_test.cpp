#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace dawn::infra::hash;

TEST(Sha256, MatchesStandardVector) {
    EXPECT_EQ(sha256_hex("abc"), "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256, HashesFilesAndComparesWithPrefixNormalization) {
    const auto root = std::filesystem::temp_directory_path() / "dawn-sha256-test";
    std::filesystem::remove_all(root);
    const auto path = root / "payload.bin";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(path, "abc", &error)) << error;

    const auto digest = sha256_file_hex(path, &error);
    ASSERT_FALSE(digest.empty()) << error;
    EXPECT_EQ(digest, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_TRUE(compare_hash("sha256:BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD", digest));
    EXPECT_TRUE(compare_hash("  ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad  ", "SHA256:BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD"));

    std::filesystem::remove_all(root);
}
