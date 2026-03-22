#pragma once

#include <cctype>
#include <deque>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace dawn::infra::net {

enum class HttpMethod {
    Get,
    Post,
    Put,
    Patch,
    Delete,
};

inline std::string to_string(HttpMethod method) {
    switch (method) {
    case HttpMethod::Get: return "GET";
    case HttpMethod::Post: return "POST";
    case HttpMethod::Put: return "PUT";
    case HttpMethod::Patch: return "PATCH";
    case HttpMethod::Delete: return "DELETE";
    }
    return "GET";
}

struct HttpRequest {
    HttpMethod method = HttpMethod::Get;
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int statusCode = 200;
    std::map<std::string, std::string> headers;
    std::string body;

    [[nodiscard]] bool success() const noexcept {
        return statusCode >= 200 && statusCode < 300;
    }
};

class HttpClient {
public:
    virtual ~HttpClient() = default;
    virtual HttpResponse send(const HttpRequest& request) = 0;
};

inline std::string url_encode(std::string_view text, bool spaceAsPlus = false) {
    std::ostringstream out;
    out << std::uppercase << std::hex;
    for (const unsigned char ch : text) {
        const bool unreserved =
            (ch >= 'A' && ch <= 'Z') ||
            (ch >= 'a' && ch <= 'z') ||
            (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~';
        if (unreserved) {
            out << static_cast<char>(ch);
        } else if (spaceAsPlus && ch == ' ') {
            out << '+';
        } else {
            out << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
        }
    }
    return out.str();
}

inline std::string append_query(const std::string& base, const std::vector<std::pair<std::string, std::string>>& params) {
    if (params.empty()) {
        return base;
    }

    std::string result = base;
    result += base.find('?') == std::string::npos ? '?' : '&';
    for (std::size_t index = 0; index < params.size(); ++index) {
        if (index > 0) {
            result += '&';
        }
        result += url_encode(params[index].first);
        result += '=';
        result += url_encode(params[index].second);
    }
    return result;
}

inline std::string form_encode(const std::vector<std::pair<std::string, std::string>>& fields) {
    std::string result;
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0) {
            result += '&';
        }
        result += url_encode(fields[index].first, true);
        result += '=';
        result += url_encode(fields[index].second, true);
    }
    return result;
}

class InMemoryHttpClient final : public HttpClient {
public:
    void set_response(HttpMethod method, std::string url, HttpResponse response) {
        routes_[route_key(method, url)] = std::move(response);
    }

    void set_default_response(HttpResponse response) {
        defaultResponse_ = std::move(response);
    }

    const std::vector<HttpRequest>& requests() const noexcept {
        return requests_;
    }

    void clear() {
        requests_.clear();
        routes_.clear();
    }

    HttpResponse send(const HttpRequest& request) override {
        requests_.push_back(request);
        const auto it = routes_.find(route_key(request.method, request.url));
        if (it != routes_.end()) {
            return it->second;
        }
        return defaultResponse_;
    }

private:
    static std::string route_key(HttpMethod method, const std::string& url) {
        return to_string(method) + "|" + url;
    }

    std::map<std::string, HttpResponse> routes_;
    HttpResponse defaultResponse_{404, {}, R"({"error":"not_found"})"};
    std::vector<HttpRequest> requests_;
};

class FakeHttpClient final : public HttpClient {
public:
    void push_response(HttpResponse response) {
        responses_.push_back(std::move(response));
    }

    void set_default_response(HttpResponse response) {
        defaultResponse_ = std::move(response);
    }

    const std::vector<HttpRequest>& requests() const noexcept {
        return requests_;
    }

    void clear() {
        requests_.clear();
        responses_.clear();
    }

    HttpResponse send(const HttpRequest& request) override {
        requests_.push_back(request);
        if (!responses_.empty()) {
            auto response = std::move(responses_.front());
            responses_.pop_front();
            return response;
        }
        return defaultResponse_;
    }

private:
    std::deque<HttpResponse> responses_;
    HttpResponse defaultResponse_{500, {}, R"({"error":"no_response_configured"})"};
    std::vector<HttpRequest> requests_;
};

} // namespace dawn::infra::net
