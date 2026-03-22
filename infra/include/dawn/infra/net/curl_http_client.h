#pragma once

#include "dawn/infra/net/http_client.h"

#include <memory>

namespace dawn::infra::net {

class CurlHttpClient final : public HttpClient {
public:
    CurlHttpClient();
    ~CurlHttpClient() override;

    HttpResponse send(const HttpRequest& request) override;
};

} // namespace dawn::infra::net
