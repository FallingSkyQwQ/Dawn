#pragma once

#include "dawn/infra/net/http_client.h"

#ifdef _WIN32

#include <windows.h>
#include <winhttp.h>

#include <string>

namespace dawn::infra::net {

class WinHttpClient final : public HttpClient {
public:
    WinHttpClient();
    ~WinHttpClient() override;

    HttpResponse send(const HttpRequest& request) override;

private:
    [[nodiscard]] static std::wstring utf8_to_wstring(const std::string& text);
    [[nodiscard]] static std::string wstring_to_utf8(const std::wstring& text);
    [[nodiscard]] static std::wstring method_to_verb(HttpMethod method);
    [[nodiscard]] static std::wstring normalize_path(const std::wstring& path, const std::wstring& extra);
    [[nodiscard]] static std::wstring format_headers(const HttpRequest& request);
    [[nodiscard]] static std::map<std::string, std::string> parse_headers(const std::wstring& rawHeaders);
    [[nodiscard]] static std::string error_message(const std::string& prefix);

    HINTERNET session_ = nullptr;
};

} // namespace dawn::infra::net

#endif
