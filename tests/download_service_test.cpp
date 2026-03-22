#include "dawn/core/download/download_service.h"
#include "dawn/core/service/task_queue.h"
#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/hash/sha256.h"
#include "dawn/infra/net/http_client.h"

#include <gtest/gtest.h>

#include <chrono>
#include <algorithm>
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

TEST(DownloadService, DownloadsFileInSequentialChunks) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{206, {{"Content-Range", "bytes 0-3/11"}}, "hell"});
    client->push_response(dawn::infra::net::HttpResponse{206, {{"Content-Range", "bytes 4-7/11"}}, "o wo"});
    client->push_response(dawn::infra::net::HttpResponse{206, {{"Content-Range", "bytes 8-10/11"}}, "rld"});

    const std::vector<std::string> expectedRanges = {"bytes=0-3", "bytes=4-7", "bytes=8-10"};
    std::size_t requestIndex = 0;
    client->set_request_validator([&](const dawn::infra::net::HttpRequest& request) -> std::optional<std::string> {
        if (requestIndex >= expectedRanges.size()) {
            return std::string("unexpected extra request");
        }
        const auto it = request.headers.find("Range");
        if (it == request.headers.end()) {
            return std::string("missing Range header");
        }
        if (it->second != expectedRanges[requestIndex]) {
            return std::string("unexpected Range header: " + it->second);
        }
        ++requestIndex;
        return std::nullopt;
    });

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-chunks";
    std::filesystem::remove_all(root);
    const auto destination = root / "artifact.bin";

    DownloadService service(client);
    DownloadRequest request;
    request.id = "download-chunked";
    request.title = "Chunked Artifact";
    request.url = "https://example.invalid/artifact.bin";
    request.destination = destination;
    request.chunkSizeBytes = 4;
    request.expectedHash = dawn::infra::hash::sha256_hex("hello world");

    const auto result = service.execute(request);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.state, DownloadState::Completed);
    EXPECT_EQ(result.chunks.size(), 3u);
    EXPECT_TRUE(client->validation_errors().empty());
    EXPECT_EQ(read_file(destination), "hello world");
    EXPECT_EQ(result.artifact.bytesWritten, std::string("hello world").size());
    EXPECT_EQ(result.chunks.front().chunk.index, 0u);
    EXPECT_EQ(result.chunks.back().chunk.index, 2u);

    std::filesystem::remove_all(root);
}

TEST(DownloadService, ThrottlesChunkDownloadsUsingConfiguredRate) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{206, {{"Content-Range", "bytes 0-1/6"}}, "ab"});
    client->push_response(dawn::infra::net::HttpResponse{206, {{"Content-Range", "bytes 2-3/6"}}, "cd"});
    client->push_response(dawn::infra::net::HttpResponse{206, {{"Content-Range", "bytes 4-5/6"}}, "ef"});

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-throttle";
    std::filesystem::remove_all(root);
    const auto destination = root / "artifact.bin";

    std::vector<std::chrono::milliseconds> sleeps;
    DownloadService service(client);
    service.set_bytes_per_second(2);
    service.set_sleeper([&](std::chrono::milliseconds duration) {
        sleeps.push_back(duration);
    });

    DownloadRequest request;
    request.id = "download-throttle";
    request.title = "Throttle Artifact";
    request.url = "https://example.invalid/artifact.bin";
    request.destination = destination;
    request.chunkSizeBytes = 2;
    request.expectedHash = dawn::infra::hash::sha256_hex("abcdef");

    const auto result = service.execute(request);

    EXPECT_TRUE(result.success);
    ASSERT_EQ(sleeps.size(), 3u);
    EXPECT_EQ(sleeps[0], std::chrono::milliseconds(1000));
    EXPECT_EQ(sleeps[1], std::chrono::milliseconds(1000));
    EXPECT_EQ(sleeps[2], std::chrono::milliseconds(1000));
    EXPECT_EQ(read_file(destination), "abcdef");

    std::filesystem::remove_all(root);
}

TEST(DownloadService, FallsBackToMirrorAfterPrimaryFailure) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{500, {}, "primary failure"});
    client->push_response(dawn::infra::net::HttpResponse{200, {}, "mirror payload"});

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-mirror";
    std::filesystem::remove_all(root);
    const auto destination = root / "artifact.bin";

    DownloadService service(client, 2);
    DownloadRequest request;
    request.id = "download-5";
    request.title = "Mirror Artifact";
    request.url = "https://primary.invalid/artifact.bin";
    request.mirrors = {"https://mirror.invalid/artifact.bin"};
    request.destination = destination;
    request.expectedHash = dawn::infra::hash::sha256_hex("mirror payload");

    const auto result = service.execute(request);

    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.artifact.sourceUrl, "https://mirror.invalid/artifact.bin");
    EXPECT_EQ(read_file(destination), "mirror payload");
    ASSERT_FALSE(result.logs.empty());
    EXPECT_TRUE(std::any_of(result.logs.begin(), result.logs.end(), [](const std::string& log) {
        return log.find("download source: https://mirror.invalid/artifact.bin") != std::string::npos;
    }));
    EXPECT_NE(std::find(result.logs.begin(), result.logs.end(), "falling back to next mirror"), result.logs.end());

    std::filesystem::remove_all(root);
}

TEST(DownloadService, ExecutesBatchDownloadsConcurrently) {
    auto client = std::make_shared<dawn::infra::net::InMemoryHttpClient>();

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-batch";
    std::filesystem::remove_all(root);

    std::vector<DownloadRequest> requests;
    for (int index = 1; index <= 3; ++index) {
        const auto url = "https://example.invalid/artifact-" + std::to_string(index) + ".bin";
        client->set_response(
            dawn::infra::net::HttpMethod::Get,
            url,
            dawn::infra::net::HttpResponse{200, {}, "payload-" + std::to_string(index)});

        DownloadRequest request;
        request.id = "batch-" + std::to_string(index);
        request.title = "Batch " + std::to_string(index);
        request.url = url;
        request.destination = root / ("artifact-" + std::to_string(index) + ".bin");
        request.expectedHash = dawn::infra::hash::sha256_hex("payload-" + std::to_string(index));
        requests.push_back(std::move(request));
    }

    DownloadService service(client, 3);
    TaskQueue queue;
    const auto batch = service.execute_many(requests, &queue);

    ASSERT_EQ(batch.results.size(), 3u);
    for (std::size_t index = 0; index < batch.results.size(); ++index) {
        EXPECT_TRUE(batch.results[index].success) << batch.results[index].error;
        EXPECT_TRUE(std::filesystem::exists(requests[index].destination)) << batch.results[index].error;
        EXPECT_EQ(read_file(requests[index].destination), "payload-" + std::to_string(index + 1)) << batch.results[index].error;
    }
    ASSERT_EQ(queue.tasks().size(), 3u);
    for (const auto& task : queue.tasks()) {
        EXPECT_EQ(task.status, TaskStatus::Succeeded);
    }

    std::filesystem::remove_all(root);
}

TEST(DownloadService, BatchFailureDoesNotBlockOtherTasks) {
    auto client = std::make_shared<dawn::infra::net::InMemoryHttpClient>();
    client->set_response(
        dawn::infra::net::HttpMethod::Get,
        "https://example.invalid/ok-1.bin",
        dawn::infra::net::HttpResponse{200, {}, "ok-1"});
    client->set_response(
        dawn::infra::net::HttpMethod::Get,
        "https://example.invalid/ok-2.bin",
        dawn::infra::net::HttpResponse{200, {}, "ok-2"});

    const auto root = std::filesystem::temp_directory_path() / "dawn-download-batch-fail";
    std::filesystem::remove_all(root);

    std::vector<DownloadRequest> requests;
    DownloadRequest ok1;
    ok1.id = "batch-ok-1";
    ok1.title = "OK 1";
    ok1.url = "https://example.invalid/ok-1.bin";
    ok1.destination = root / "ok-1.bin";
    ok1.expectedHash = dawn::infra::hash::sha256_hex("ok-1");
    requests.push_back(ok1);

    DownloadRequest bad;
    bad.id = "batch-bad";
    bad.title = "Bad";
    bad.url = "https://example.invalid/missing.bin";
    bad.destination = root / "bad.bin";
    requests.push_back(bad);

    DownloadRequest ok2;
    ok2.id = "batch-ok-2";
    ok2.title = "OK 2";
    ok2.url = "https://example.invalid/ok-2.bin";
    ok2.destination = root / "ok-2.bin";
    ok2.expectedHash = dawn::infra::hash::sha256_hex("ok-2");
    requests.push_back(ok2);

    DownloadService service(client, 2);
    TaskQueue queue;
    const auto batch = service.execute_many(requests, &queue);

    ASSERT_EQ(batch.results.size(), 3u);
    EXPECT_TRUE(batch.results[0].success) << batch.results[0].error;
    EXPECT_FALSE(batch.results[1].success);
    EXPECT_TRUE(batch.results[2].success) << batch.results[2].error;
    EXPECT_TRUE(std::filesystem::exists(requests[0].destination));
    EXPECT_FALSE(std::filesystem::exists(requests[1].destination));
    EXPECT_TRUE(std::filesystem::exists(requests[2].destination));
    ASSERT_EQ(queue.tasks().size(), 3u);
    std::size_t successCount = 0;
    std::size_t failureCount = 0;
    for (const auto& task : queue.tasks()) {
        if (task.status == TaskStatus::Succeeded) {
            ++successCount;
        } else if (task.status == TaskStatus::Failed) {
            ++failureCount;
        }
    }
    EXPECT_EQ(successCount, 2u);
    EXPECT_EQ(failureCount, 1u);

    std::filesystem::remove_all(root);
}
