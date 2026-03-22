#pragma once

#include "dawn/infra/net/http_client.h"

#include <memory>

namespace dawn::infra::net {

class HttpClientFactory {
public:
    static std::shared_ptr<HttpClient> create_default_http_client();
};

} // namespace dawn::infra::net
