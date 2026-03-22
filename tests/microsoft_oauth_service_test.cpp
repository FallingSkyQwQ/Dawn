#include "dawn/core/auth/microsoft_oauth_service.h"

#include <gtest/gtest.h>

#include <memory>

using namespace dawn::core;

TEST(MicrosoftOAuthServiceProtocol, StartsDeviceCodeFlowWithExpectedPayload) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "device_code": "device-1",
            "user_code": "ABCD-EFGH",
            "verification_uri": "https://microsoft.com/devicelogin",
            "verification_uri_complete": "https://microsoft.com/devicelogin?code=ABCD-EFGH",
            "expires_in": 900,
            "interval": 5,
            "message": "Go to microsoft.com/devicelogin"
        })"
    });

    MicrosoftOAuthService service(client);
    DeviceCodeRequest request;
    request.clientId = "client-123";
    request.scopes = {"openid", "profile"};

    std::string error;
    const auto response = service.start_device_code_flow(request, &error);
    ASSERT_TRUE(response.ok) << error;
    ASSERT_EQ(client->requests().size(), 1u);
    const auto& httpRequest = client->requests().front();
    EXPECT_EQ(httpRequest.method, dawn::infra::net::HttpMethod::Post);
    EXPECT_NE(httpRequest.url.find("https://login.microsoftonline.com/common/oauth2/v2.0/devicecode"), std::string::npos);
    EXPECT_NE(httpRequest.body.find("client_id=client-123"), std::string::npos);
    EXPECT_NE(httpRequest.body.find("scope=openid+profile+offline_access"), std::string::npos);
    EXPECT_EQ(response.deviceCode, "device-1");
    EXPECT_EQ(response.userCode, "ABCD-EFGH");
    EXPECT_EQ(response.verificationUri, "https://microsoft.com/devicelogin");
    EXPECT_EQ(response.interval, 5);
}

TEST(MicrosoftOAuthServiceProtocol, PollsAndRefreshesTokens) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "token_type": "Bearer",
            "scope": "openid profile email offline_access",
            "expires_in": 3599,
            "access_token": "access-1",
            "refresh_token": "refresh-1",
            "id_token": "id-1"
        })"
    });
    client->push_response(dawn::infra::net::HttpResponse{
        200,
        {},
        R"({
            "token_type": "Bearer",
            "scope": "openid profile email offline_access",
            "expires_in": 3599,
            "access_token": "access-2",
            "refresh_token": "refresh-2"
        })"
    });

    MicrosoftOAuthService service(client);
    DeviceCodeRequest request;
    request.clientId = "client-123";

    std::string error;
    const auto polled = service.poll_token(request, "device-1", &error);
    ASSERT_TRUE(polled.ok) << error;
    const auto refreshed = service.refresh_token(request, "refresh-1", &error);
    ASSERT_TRUE(refreshed.ok) << error;

    ASSERT_EQ(client->requests().size(), 2u);
    EXPECT_NE(client->requests()[0].body.find("grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Adevice_code"), std::string::npos);
    EXPECT_NE(client->requests()[0].body.find("device_code=device-1"), std::string::npos);
    EXPECT_NE(client->requests()[1].body.find("grant_type=refresh_token"), std::string::npos);
    EXPECT_NE(client->requests()[1].body.find("refresh_token=refresh-1"), std::string::npos);
    EXPECT_EQ(polled.accessToken, "access-1");
    EXPECT_EQ(polled.refreshToken, "refresh-1");
    EXPECT_EQ(refreshed.accessToken, "access-2");
    EXPECT_EQ(refreshed.refreshToken, "refresh-2");
}

TEST(MicrosoftOAuthServiceProtocol, MapsRetryableErrorCodesToReadableMessages) {
    auto client = std::make_shared<dawn::infra::net::FakeHttpClient>();
    client->push_response(dawn::infra::net::HttpResponse{
        400,
        {},
        R"({
            "error": "authorization_pending"
        })"
    });

    MicrosoftOAuthService service(client);
    DeviceCodeRequest request;
    request.clientId = "client-123";

    std::string error;
    const auto token = service.poll_token(request, "device-1", &error);
    EXPECT_FALSE(token.ok);
    EXPECT_TRUE(token.retryable);
    EXPECT_EQ(token.errorCode, "authorization_pending");
    EXPECT_NE(error.find("completed Microsoft sign-in"), std::string::npos);
    EXPECT_EQ(MicrosoftOAuthService::describe_error("authorization_pending"), "The user has not completed Microsoft sign-in yet.");
}
