#include "dawn/infra/net/http_client_factory.h"

#if defined(DAWN_HAS_CURL)
#include "dawn/infra/net/curl_http_client.h"
#elif defined(_WIN32)
#include "dawn/infra/net/win_http_client.h"
#endif

namespace dawn::infra::net {

std::shared_ptr<HttpClient> HttpClientFactory::create_default_http_client() {
#if defined(DAWN_HAS_CURL)
    return std::make_shared<CurlHttpClient>();
#elif defined(_WIN32)
    return std::make_shared<WinHttpClient>();
#else
    return nullptr;
#endif
}

} // namespace dawn::infra::net
