#include "dawn/core/download/download_service.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"
#include "dawn/infra/net/http_client.h"

#include <gtest/gtest.h>

#include <filesystem>

using namespace dawn::core;

namespace {

std::string read_file(const std::filesystem::path& path) {
    std::string text;
    std::string error;
    EXPECT_TRUE(dawn::infra::fs::read_text_file(path, &text, &error)) << error;
    return text;
}

} // namespace

TEST(DownloadService, WritesDownloadedFile) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "downloaded payload"});

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-success";
    std::filesystem::remove_all(root);
    const auto destination = root / "artifact.bin";

    DownloadService service(client);
    DownloadRequest request;
    request.id = "download-1";
    request.title = "Demo Artifact";
    request.url = "https://example.invalid/artifact.bin";
    request.destination = destination;
    request.expectedHash = dawn::infra::hash::sha256_hex("downloaded payload");

    TaskQueue queue;
    const auto result = service.execute(request, &queue);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.state, DownloadState::Completed);
    EXPECT_EQ(result.plan.status, TaskStatus::Succeeded);
    EXPECT_TRUE(result.artifact.verified);
    EXPECT_EQ(result.artifact.bytesWritten, std::string("downloaded payload").size());
    EXPECT_EQ(read_file(destination), "downloaded payload");
    ASSERT_EQ(queue.tasks().size(), 1u);
    EXPECT_EQ(queue.tasks().front().status, TaskStatus::Succeeded);
    EXPECT_FALSE(result.logs.empty());

    std::filesystem::remove_all(root);
}

TEST(DownloadService, RetriesAfterInitialFailure) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{500, {}, "temporary failure"});
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "retry payload"});

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-retry";
    std::filesystem::remove_all(root);
    const auto destination = root / "artifact.bin";

    DownloadService service(client);
    DownloadRequest request;
    request.id = "download-2";
    request.title = "Retry Artifact";
    request.url = "https://example.invalid/artifact.bin";
    request.destination = destination;
    request.retryCount = 1;

    const auto result = service.execute(request);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.artifact.attempts, 2);
    EXPECT_EQ(result.state, DownloadState::Completed);
    EXPECT_EQ(result.plan.status, TaskStatus::Succeeded);
    EXPECT_EQ(read_file(destination), "retry payload");
    EXPECT_FALSE(result.logs.empty());

    std::filesystem::remove_all(root);
}

TEST(DownloadService, FailsOnHashMismatch) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "payload"});

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-hash";
    std::filesystem::remove_all(root);
    const auto destination = root / "artifact.bin";

    DownloadService service(client);
    DownloadRequest request;
    request.id = "download-3";
    request.title = "Hash Artifact";
    request.url = "https://example.invalid/artifact.bin";
    request.destination = destination;
    request.expectedHash = "deadbeef";

    const auto result = service.execute(request);

    EXPECT_FALSE(result.success);
    EXPECT_EQ(result.state, DownloadState::Failed);
    EXPECT_EQ(result.plan.status, TaskStatus::Failed);
    EXPECT_EQ(result.error, "hash mismatch");
    EXPECT_FALSE(result.artifact.checksum.empty());
    EXPECT_FALSE(result.artifact.verified);

    std::filesystem::remove_all(root);
}

TEST(DownloadService, ResumesPartialDownloadsWithRangeHeader) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{206, {}, "world"});
    client->expect_header("Range", "bytes=6-");

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-resume";
    std::filesystem::remove_all(root);
    const auto destination = root / "artifact.bin";

    std::string error;
    ASSERT_TRUE(dawn::infra::fs::write_binary_file(destination, "hello ", &error)) << error;

    DownloadService service(client);
    DownloadRequest request;
    request.id = "download-4";
    request.title = "Resume Artifact";
    request.url = "https://example.invalid/artifact.bin";
    request.destination = destination;
    request.overwriteExisting = false;
    request.expectedHash = dawn::infra::hash::sha256_hex("hello world");

    const auto result = service.execute(request);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.state, DownloadState::Completed);
    EXPECT_TRUE(client->expectations_met()) << (client->validation_errors().empty() ? std::string() : client->validation_errors().front());
    EXPECT_EQ(read_file(destination), "hello world");
    EXPECT_EQ(result.artifact.bytesWritten, std::string("hello world").size());
    EXPECT_EQ(result.artifact.checksum, dawn::infra::hash::sha256_file_hex(destination));

    std::filesystem::remove_all(root);
}
