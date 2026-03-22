#ifdef _WIN32

#include "dawn/infra/net/win_http_client.h"

#include <algorithm>
#include <string_view>
#include <utility>

namespace dawn::infra::net {

WinHttpClient::WinHttpClient()
    : session_(WinHttpOpen(L"Dawn/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0)) {
}

WinHttpClient::~WinHttpClient() {
    if (session_) {
        WinHttpCloseHandle(session_);
        session_ = nullptr;
    }
}

std::wstring WinHttpClient::utf8_to_wstring(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const auto required = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0);
    if (required <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(required), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), required);
    return result;
}

std::string WinHttpClient::wstring_to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const auto required = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(required), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), result.data(), required, nullptr, nullptr);
    return result;
}

std::wstring WinHttpClient::method_to_verb(HttpMethod method) {
    switch (method) {
    case HttpMethod::Get: return L"GET";
    case HttpMethod::Post: return L"POST";
    case HttpMethod::Put: return L"PUT";
    case HttpMethod::Patch: return L"PATCH";
    case HttpMethod::Delete: return L"DELETE";
    }
    return L"GET";
}

std::wstring WinHttpClient::normalize_path(const std::wstring& path, const std::wstring& extra) {
    if (path.empty()) {
        return L"/";
    }
    return path + extra;
}

std::wstring WinHttpClient::format_headers(const HttpRequest& request) {
    std::wstring result;
    for (const auto& [name, value] : request.headers) {
        result += utf8_to_wstring(name);
        result += L": ";
        result += utf8_to_wstring(value);
        result += L"\r\n";
    }
    return result;
}

std::map<std::string, std::string> WinHttpClient::parse_headers(const std::wstring& rawHeaders) {
    std::map<std::string, std::string> result;
    std::wstring current;
    for (std::size_t index = 0; index <= rawHeaders.size(); ++index) {
        const wchar_t ch = index < rawHeaders.size() ? rawHeaders[index] : L'\n';
        if (ch == L'\r') {
            continue;
        }
        if (ch != L'\n') {
            current.push_back(ch);
            continue;
        }
        if (current.empty()) {
            continue;
        }
        const auto colon = current.find(L':');
        if (colon != std::wstring::npos) {
            const auto name = current.substr(0, colon);
            auto value = current.substr(colon + 1);
            value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](wchar_t ch) {
                return ch != L' ' && ch != L'\t';
            }));
            result.emplace(wstring_to_utf8(name), wstring_to_utf8(value));
        }
        current.clear();
    }
    return result;
}

std::string WinHttpClient::error_message(const std::string& prefix) {
    const auto code = GetLastError();
    return prefix + " (WinHTTP error " + std::to_string(code) + ")";
}

HttpResponse WinHttpClient::send(const HttpRequest& request) {
    HttpResponse response;

    if (!session_) {
        response.statusCode = 0;
        response.body = "WinHTTP session is not available";
        return response;
    }

    const auto url = utf8_to_wstring(request.url);
    URL_COMPONENTS components{};
    components.dwStructSize = sizeof(components);
    components.dwSchemeLength = static_cast<DWORD>(-1);
    components.dwHostNameLength = static_cast<DWORD>(-1);
    components.dwUrlPathLength = static_cast<DWORD>(-1);
    components.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!WinHttpCrackUrl(url.c_str(), 0, 0, &components)) {
        response.statusCode = 400;
        response.body = error_message("failed to parse request url");
        return response;
    }

    const std::wstring host(components.lpszHostName, components.dwHostNameLength);
    const std::wstring path = normalize_path(
        components.lpszUrlPath ? std::wstring(components.lpszUrlPath, components.dwUrlPathLength) : std::wstring(),
        components.lpszExtraInfo ? std::wstring(components.lpszExtraInfo, components.dwExtraInfoLength) : std::wstring());

    const HINTERNET connectHandle = WinHttpConnect(session_, host.c_str(), components.nPort, 0);
    if (!connectHandle) {
        response.statusCode = 0;
        response.body = error_message("failed to connect");
        return response;
    }

    const HINTERNET requestHandle = WinHttpOpenRequest(
        connectHandle,
        method_to_verb(request.method).c_str(),
        path.c_str(),
        nullptr,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        components.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0);
    if (!requestHandle) {
        WinHttpCloseHandle(connectHandle);
        response.statusCode = 0;
        response.body = error_message("failed to open request");
        return response;
    }

    const auto headers = format_headers(request);
    const BYTE* bodyData = request.body.empty() ? WINHTTP_NO_REQUEST_DATA : reinterpret_cast<const BYTE*>(request.body.data());
    const DWORD bodySize = static_cast<DWORD>(request.body.size());
    const auto headersPtr = headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str();
    const auto headersLength = headers.empty() ? 0 : static_cast<DWORD>(-1);

    if (!WinHttpSendRequest(
            requestHandle,
            headersPtr,
            headersLength,
            request.body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<LPVOID>(reinterpret_cast<const void*>(bodyData)),
            bodySize,
            bodySize,
            0)) {
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectHandle);
        response.statusCode = 0;
        response.body = error_message("failed to send request");
        return response;
    }

    if (!WinHttpReceiveResponse(requestHandle, nullptr)) {
        WinHttpCloseHandle(requestHandle);
        WinHttpCloseHandle(connectHandle);
        response.statusCode = 0;
        response.body = error_message("failed to receive response");
        return response;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (WinHttpQueryHeaders(
            requestHandle,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &statusSize,
            WINHTTP_NO_HEADER_INDEX)) {
        response.statusCode = static_cast<int>(statusCode);
    }

    DWORD rawSize = 0;
    SetLastError(ERROR_SUCCESS);
    const bool rawHeaderQuery = WinHttpQueryHeaders(
        requestHandle,
        WINHTTP_QUERY_RAW_HEADERS_CRLF,
        WINHTTP_HEADER_NAME_BY_INDEX,
        nullptr,
        &rawSize,
        WINHTTP_NO_HEADER_INDEX);
    if (!rawHeaderQuery && GetLastError() == ERROR_INSUFFICIENT_BUFFER && rawSize > 0) {
        std::wstring rawHeaders;
        rawHeaders.resize(rawSize / sizeof(wchar_t));
        if (WinHttpQueryHeaders(
                requestHandle,
                WINHTTP_QUERY_RAW_HEADERS_CRLF,
                WINHTTP_HEADER_NAME_BY_INDEX,
                rawHeaders.data(),
                &rawSize,
                WINHTTP_NO_HEADER_INDEX)) {
            if (!rawHeaders.empty() && rawHeaders.back() == L'\0') {
                rawHeaders.pop_back();
            }
            response.headers = parse_headers(rawHeaders);
        }
    }

    std::string body;
    for (;;) {
        DWORD available = 0;
        if (!WinHttpQueryDataAvailable(requestHandle, &available)) {
            WinHttpCloseHandle(requestHandle);
            WinHttpCloseHandle(connectHandle);
            response.statusCode = 0;
            response.body = error_message("failed to query response data");
            return response;
        }
        if (available == 0) {
            break;
        }

        std::string chunk(static_cast<std::size_t>(available), '\0');
        DWORD read = 0;
        if (!WinHttpReadData(
                requestHandle,
                chunk.data(),
                available,
                &read)) {
            WinHttpCloseHandle(requestHandle);
            WinHttpCloseHandle(connectHandle);
            response.statusCode = 0;
            response.body = error_message("failed to read response data");
            return response;
        }
        chunk.resize(read);
        body += chunk;
    }

    WinHttpCloseHandle(requestHandle);
    WinHttpCloseHandle(connectHandle);
    response.body = std::move(body);
    return response;
}

} // namespace dawn::infra::net

#endif
