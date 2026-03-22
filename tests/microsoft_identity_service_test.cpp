#include "dawn/core/auth/account_service.h"
#include "dawn/core/auth/microsoft_identity_service.h"
#include "dawn/infra/net/http_client.h"

#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <memory>

using namespace dawn::core;

namespace {

constexpr auto kFixedNow = std::chrono::system_clock::time_point{};

} // namespace

TEST(MicrosoftIdentityService, AuthenticatesFullChainAndUpdatesAccountCache) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "Token": "xbl-token",
            "DisplayClaims": {
                "xui": [
                    { "uhs": "user-hash" }
                ]
            },
            "IssueInstant": "1970-01-01T00:00:00.000Z",
            "NotAfter": "1970-01-01T01:00:00.000Z"
        })"
    });
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "Token": "xsts-token",
            "DisplayClaims": {
                "xui": [
                    { "uhs": "user-hash" }
                ]
            }
        })"
    });
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "token_type": "Bearer",
            "expires_in": 3600,
            "access_token": "minecraft-token"
        })"
    });
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "id": "uuid-123",
            "name": "Dawn Player"
        })"
    });

    MicrosoftIdentityService service(client);
    MicrosoftIdentityRequest request;
    request.oauthAccessToken = "oauth-token";

    std::string error;
    const auto result = service.authenticate(request, &error, kFixedNow);
    ASSERT_TRUE(result.ok) << error;
    ASSERT_EQ(client->requests().size(), 4u);
    EXPECT_EQ(result.xblToken, "xbl-token");
    EXPECT_EQ(result.xstsToken, "xsts-token");
    EXPECT_EQ(result.userHash, "user-hash");
    EXPECT_EQ(result.minecraftAccessToken, "minecraft-token");
    EXPECT_EQ(result.uuid, "uuid-123");
    EXPECT_EQ(result.displayName, "Dawn Player");
    EXPECT_FALSE(result.expiresAt.empty());

    const auto root = std::filesystem::temp_directory_path() / "dawn-identity-account-cache";
    std::filesystem::remove_all(root);

    AccountService accounts(root);
    MicrosoftAccount account;
    account.accountId = "minecraft-user";
    account.accessToken = "oauth-token";
    account.expiresAt = "1970-01-01T02:00:00";

    auto profile = accounts.add_microsoft_account(account, &error);
    ASSERT_FALSE(profile.id.empty()) << error;
    ASSERT_TRUE(accounts.update_microsoft_account(profile.id, result, &error, kFixedNow)) << error;

    const auto loaded = accounts.accounts(&error);
    ASSERT_EQ(loaded.size(), 1u);
    EXPECT_EQ(loaded.front().microsoft.uuid, "uuid-123");
    EXPECT_EQ(loaded.front().microsoft.displayName, "Dawn Player");
    EXPECT_EQ(loaded.front().microsoft.expiresAt, result.expiresAt);
    EXPECT_EQ(loaded.front().tokenState, TokenState::Valid);

    std::filesystem::remove_all(root);
}

TEST(MicrosoftIdentityService, StopsAtXstsFailure) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "Token": "xbl-token",
            "DisplayClaims": {
                "xui": [
                    { "uhs": "user-hash" }
                ]
            }
        })"
    });
    client->push_response(dawn::infra::net::HttpResponse{
        401,
        {},
        R"({
            "XErr": 2148916233,
            "Message": "The XSTS service rejected the account."
        })"
    });

    MicrosoftIdentityService service(client);
    MicrosoftIdentityRequest request;
    request.oauthAccessToken = "oauth-token";

    std::string error;
    const auto result = service.authenticate(request, &error, kFixedNow);
    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.errorStage, "xsts");
    EXPECT_EQ(client->requests().size(), 2u);
    EXPECT_NE(error.find("XSTS"), std::string::npos);
    EXPECT_NE(result.errorMessage.find("XSTS"), std::string::npos);
}
