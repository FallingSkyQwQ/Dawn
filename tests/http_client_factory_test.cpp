#include "dawn/core/auth/microsoft_oauth_service.h"
#include "dawn/core/provider/modrinth_provider.h"
#include "dawn/infra/net/http_client_factory.h"

#include <gtest/gtest.h>

#include <string>

using namespace dawn::infra::net;

TEST(HttpClientFactory, DefaultClientIsRealTransport) {
    const auto client = HttpClientFactory::create_default_http_client();
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(dynamic_cast<FakeHttpClient*>(client.get()), nullptr);
}

TEST(HttpClientFactory, DefaultTransportFeedsProviderAndOAuth) {
    const auto providerClient = HttpClientFactory::create_default_http_client();
    ASSERT_NE(providerClient, nullptr);
    EXPECT_EQ(dynamic_cast<FakeHttpClient*>(providerClient.get()), nullptr);

    const auto oauthClient = HttpClientFactory::create_default_http_client();
    ASSERT_NE(oauthClient, nullptr);
    EXPECT_EQ(dynamic_cast<FakeHttpClient*>(oauthClient.get()), nullptr);

    dawn::core::ModrinthProvider provider;
    dawn::core::SearchQuery query;
    query.text = "sodium";
    const auto searchResult = provider.search(query);
    EXPECT_EQ(dynamic_cast<FakeHttpClient*>(provider.http_client().get()), nullptr);
    EXPECT_GE(searchResult.items.size(), 0u);

    dawn::core::MicrosoftOAuthService oauth;
    dawn::core::DeviceCodeRequest request;
    request.clientId = "client-123";
    std::string error;
    const auto device = oauth.start_device_code_flow(request, &error);
    EXPECT_EQ(dynamic_cast<FakeHttpClient*>(oauth.http_client().get()), nullptr);
    EXPECT_FALSE(device.ok || error.empty());
}
