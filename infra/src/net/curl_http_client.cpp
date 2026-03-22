#include "dawn/infra/net/curl_http_client.h"

#include <curl/curl.h>

#include <algorithm>
#include <cctype>
#include <mutex>
#include <string>

namespace dawn::infra::net {

namespace {

size_t write_body_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    const auto bytes = size * nmemb;
    if (userdata == nullptr || ptr == nullptr || bytes == 0) {
        return 0;
    }
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, bytes);
    return bytes;
}

std::string trim_copy(std::string text) {
    auto is_space = [](unsigned char ch) {
        return std::isspace(ch) != 0;
    };
    while (!text.empty() && is_space(static_cast<unsigned char>(text.front()))) {
        text.erase(text.begin());
    }
    while (!text.empty() && is_space(static_cast<unsigned char>(text.back()))) {
        text.pop_back();
    }
    return text;
}

size_t write_header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    const auto bytes = size * nitems;
    if (userdata == nullptr || buffer == nullptr || bytes == 0) {
        return 0;
    }

    std::string line(buffer, bytes);
    line = trim_copy(std::move(line));
    if (line.empty()) {
        return bytes;
    }

    const auto colon = line.find(':');
    if (colon == std::string::npos) {
        return bytes;
    }

    auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
    const auto name = trim_copy(line.substr(0, colon));
    const auto value = trim_copy(line.substr(colon + 1));
    if (!name.empty()) {
        headers->insert_or_assign(name, value);
    }
    return bytes;
}

void ensure_curl_runtime() {
    static std::once_flag once;
    std::call_once(once, [] {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    });
}

const char* method_name(HttpMethod method) {
    switch (method) {
    case HttpMethod::Get: return "GET";
    case HttpMethod::Post: return "POST";
    case HttpMethod::Put: return "PUT";
    case HttpMethod::Patch: return "PATCH";
    case HttpMethod::Delete: return "DELETE";
    }
    return "GET";
}

} // namespace

CurlHttpClient::CurlHttpClient() {
    ensure_curl_runtime();
}

CurlHttpClient::~CurlHttpClient() = default;

HttpResponse CurlHttpClient::send(const HttpRequest& request) {
    HttpResponse response;

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
        response.statusCode = 0;
        response.body = "curl initialization failed";
        return response;
    }

    struct curl_slist* headerList = nullptr;
    for (const auto& [name, value] : request.headers) {
        const auto line = name + ": " + value;
        headerList = curl_slist_append(headerList, line.c_str());
    }

    std::string body;
    curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Dawn/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_body_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, write_header_callback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response.headers);
    curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method_name(request.method));

    if (!request.body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(request.body.size()));
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
    }

    if (headerList != nullptr) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }

    const auto result = curl_easy_perform(curl);
    if (result != CURLE_OK) {
        response.statusCode = 0;
        response.body = curl_easy_strerror(result);
    } else {
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        response.statusCode = static_cast<int>(status);
        response.body = std::move(body);
    }

    if (headerList != nullptr) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);
    return response;
}

} // namespace dawn::infra::net
