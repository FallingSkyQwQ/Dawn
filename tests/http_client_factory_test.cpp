#include "dawn/core/auth/microsoft_oauth_service.h"
#include "dawn/core/provider/modrinth_provider.h"
#include "dawn/infra/net/http_client_factory.h"

#include <gtest/gtest.h>

#include <string>

using namespace dawn::infra::net;

TEST(HttpClientFactory, DefaultsToFakeOnNonWindows) {
#ifdef _WIN32
    GTEST_SKIP() << "Non-Windows fallback test only.";
#else
    const auto client = HttpClientFactory::create_default_http_client();
    ASSERT_NE(client, nullptr);

    const auto* fake = dynamic_cast<FakeHttpClient*>(client.get());
    ASSERT_NE(fake, nullptr);

    HttpRequest request;
    request.url = "https://example.invalid";
    client->send(request);

    EXPECT_EQ(fake->requests().size(), 1u);
#endif
}

TEST(HttpClientFactory, DefaultTransportFeedsProviderAndOAuthOnNonWindows) {
#ifdef _WIN32
    GTEST_SKIP() << "Non-Windows fallback integration test only.";
#else
    const auto providerClient = HttpClientFactory::create_default_http_client();
    ASSERT_NE(providerClient, nullptr);
    ASSERT_NE(dynamic_cast<FakeHttpClient*>(providerClient.get()), nullptr);

    const auto oauthClient = HttpClientFactory::create_default_http_client();
    ASSERT_NE(oauthClient, nullptr);
    ASSERT_NE(dynamic_cast<FakeHttpClient*>(oauthClient.get()), nullptr);

    dawn::core::ModrinthProvider provider;
    dawn::core::SearchQuery query;
    query.text = "sodium";
    const auto searchResult = provider.search(query);

    const auto* providerFake = dynamic_cast<FakeHttpClient*>(provider.http_client().get());
    ASSERT_NE(providerFake, nullptr);
    EXPECT_EQ(providerFake->requests().size(), 1u);
    EXPECT_FALSE(searchResult.items.empty());

    dawn::core::MicrosoftOAuthService oauth;
    dawn::core::DeviceCodeRequest request;
    request.clientId = "client-123";
    std::string error;
    const auto device = oauth.start_device_code_flow(request, &error);

    const auto* oauthFake = dynamic_cast<FakeHttpClient*>(oauth.http_client().get());
    ASSERT_NE(oauthFake, nullptr);
    EXPECT_EQ(oauthFake->requests().size(), 1u);
    EXPECT_FALSE(device.ok);
    EXPECT_FALSE(error.empty());
#endif
}
