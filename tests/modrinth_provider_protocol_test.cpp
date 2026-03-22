#include "dawn/core/provider/modrinth_provider.h"

#include <gtest/gtest.h>

#include <memory>

using namespace dawn::core;

TEST(ModrinthProviderProtocol, BuildsSearchUrlAndParsesHits) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "hits": [
                {
                    "project_id": "abc123",
                    "title": "Sodium",
                    "description": "Fast rendering",
                    "author": "jellysquid",
                    "icon_url": "https://example.invalid/icon.png",
                    "date_modified": "2026-03-22T00:00:00Z",
                    "downloads": 1234,
                    "project_type": "mod",
                    "versions": ["1.20.1", "1.20.2"],
                    "categories": ["fabric", "optimization"]
                }
            ]
        })"
    });

    ModrinthProvider provider(client);
    SearchQuery query;
    query.text = "sodium";
    query.projectType = ProjectType::Modpack;
    query.categories = {"optimization"};
    query.gameVersions = {"1.20.1"};
    query.loaders = {LoaderType::Fabric, LoaderType::Quilt};
    query.clientSide = true;
    query.serverSide = true;

    const auto result = provider.search(query);
    ASSERT_EQ(client->requests().size(), 1u);
    const auto& request = client->requests().front();
    EXPECT_EQ(request.method, dawn::infra::net::HttpMethod::Get);
    EXPECT_NE(request.url.find("https://api.modrinth.com/v2/search"), std::string::npos);
    EXPECT_NE(request.url.find("query=sodium"), std::string::npos);
    EXPECT_NE(request.url.find("project_type%3Amodpack"), std::string::npos);
    EXPECT_NE(request.url.find("categories%3Aoptimization"), std::string::npos);
    EXPECT_NE(request.url.find("categories%3Afabric"), std::string::npos);
    EXPECT_NE(request.url.find("categories%3Aquilt"), std::string::npos);
    EXPECT_NE(request.url.find("versions%3A1.20.1"), std::string::npos);
    EXPECT_NE(request.url.find("client_side%3Arequired"), std::string::npos);
    EXPECT_NE(request.url.find("server_side%3Arequired"), std::string::npos);
    ASSERT_EQ(result.items.size(), 1u);
    EXPECT_EQ(result.items.front().projectId, "abc123");
    EXPECT_EQ(result.items.front().title, "Sodium");
    EXPECT_EQ(result.items.front().supportedGameVersions.size(), 2u);
    EXPECT_EQ(result.items.front().supportedLoaders.front(), LoaderType::Fabric);
}

TEST(ModrinthProviderProtocol, BuildsVersionsUrlAndParsesVersions) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"([
            {
                "id": "v1",
                "version_number": "1.0.0",
                "game_versions": ["1.20.1"],
                "loaders": ["fabric"],
                "files": [
                    {"url": "https://example.invalid/mod.jar"}
                ],
                "dependencies": [
                    {"version_id": "dep-v1"}
                ]
            }
        ])"
    });

    ModrinthProvider provider(client);
    ModrinthVersionQuery query;
    query.loaders = {"fabric"};
    query.gameVersions = {"1.20.1"};
    query.featured = true;
    query.includeChangelog = false;

    const auto versions = provider.versions("abc123", query);
    ASSERT_EQ(client->requests().size(), 1u);
    const auto& request = client->requests().front();
    EXPECT_NE(request.url.find("https://api.modrinth.com/v2/project/abc123/version"), std::string::npos);
    EXPECT_NE(request.url.find("loaders=%5B%22fabric%22%5D"), std::string::npos);
    EXPECT_NE(request.url.find("game_versions=%5B%221.20.1%22%5D"), std::string::npos);
    EXPECT_NE(request.url.find("featured=true"), std::string::npos);
    EXPECT_NE(request.url.find("include_changelog=false"), std::string::npos);
    ASSERT_EQ(versions.size(), 1u);
    EXPECT_EQ(versions.front().versionId, "v1");
    EXPECT_EQ(versions.front().name, "1.0.0");
    EXPECT_EQ(versions.front().fileUrls.front(), "https://example.invalid/mod.jar");
    EXPECT_EQ(versions.front().dependencies.front(), "dep-v1");
    EXPECT_EQ(versions.front().loaders.front(), LoaderType::Fabric);
}

