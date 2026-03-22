#include "dawn/infra/net/http_client_factory.h"

#ifdef _WIN32
#include "dawn/infra/net/win_http_client.h"
#endif

namespace dawn::infra::net {

std::shared_ptr<HttpClient> HttpClientFactory::create_default_http_client() {
#ifdef _WIN32
    return std::make_shared<WinHttpClient>();
#else
    return std::make_shared<FakeHttpClient>();
#endif
}

} // namespace dawn::infra::net
