#include "dawn/core/auth/microsoft_oauth_service.h"

#include "dawn/infra/net/http_client_factory.h"
#include "dawn/infra/json/simple_json.h"

#include <cstddef>
#include <algorithm>
#include <sstream>
#include <thread>
#include <utility>

// Minecraft Launcher Client ID
static constexpr const char* MINECRAFT_CLIENT_ID = "00000000402b5328";

namespace dawn::core {

namespace {

using dawn::infra::json::Value;
using dawn::infra::net::HttpMethod;
using dawn::infra::net::HttpRequest;
using dawn::infra::net::HttpResponse;

constexpr const char* kAuthorityHost = "https://login.microsoftonline.com";

bool has_scope(const std::vector<std::string>& scopes, const std::string& scope) {
    return std::find(scopes.begin(), scopes.end(), scope) != scopes.end();
}

std::vector<std::string> normalized_scope_list(const DeviceCodeRequest& request) {
    std::vector<std::string> scopes = request.scopes;
    if (scopes.empty()) {
        scopes = {"openid", "profile", "email", "offline_access"};
    } else if (!has_scope(scopes, "offline_access")) {
        scopes.push_back("offline_access");
    }
    return scopes;
}

std::string read_string(const Value::Object& object, const std::string& key) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_string()) {
        return value->as_string();
    }
    return {};
}

int read_int(const Value::Object& object, const std::string& key, int fallback = 0) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_number()) {
        return static_cast<int>(value->as_number());
    }
    return fallback;
}

std::string error_message_for_response(const HttpResponse& response) {
    if (!response.body.empty()) {
        const auto parsed = dawn::infra::json::parse(response.body);
        if (parsed.ok && parsed.value.is_object()) {
            const auto& object = parsed.value.as_object();
            const auto code = read_string(object, "error");
            const auto description = read_string(object, "error_description");
            if (!code.empty()) {
                return description.empty() ? MicrosoftOAuthService::describe_error(code) : description;
            }
        }
    }

    std::ostringstream out;
    out << "HTTP " << response.statusCode << " from Microsoft OAuth endpoint";
    return out.str();
}

HttpRequest make_request(dawn::infra::net::HttpMethod method, const std::string& url, const std::vector<std::pair<std::string, std::string>>& fields = {}) {
    HttpRequest request;
    request.method = method;
    request.url = url;
    request.headers.emplace("Accept", "application/json");
    if (!fields.empty()) {
        request.headers.emplace("Content-Type", "application/x-www-form-urlencoded");
        request.body = dawn::infra::net::form_encode(fields);
    }
    return request;
}

} // namespace

MicrosoftOAuthService::MicrosoftOAuthService(std::shared_ptr<dawn::infra::net::HttpClient> client)
    : client_(client ? std::move(client) : dawn::infra::net::HttpClientFactory::create_default_http_client()) {}

const std::shared_ptr<dawn::infra::net::HttpClient>& MicrosoftOAuthService::http_client() const noexcept {
    return client_;
}

std::string MicrosoftOAuthService::tenant(const DeviceCodeRequest& request) const {
    return request.tenant.empty() ? std::string("common") : request.tenant;
}

std::string MicrosoftOAuthService::device_code_url(const DeviceCodeRequest& request) const {
    return std::string(kAuthorityHost) + "/" + tenant(request) + "/oauth2/v2.0/devicecode";
}

std::string MicrosoftOAuthService::token_url(const DeviceCodeRequest& request) const {
    return std::string(kAuthorityHost) + "/" + tenant(request) + "/oauth2/v2.0/token";
}

std::vector<std::string> MicrosoftOAuthService::normalized_scopes(const DeviceCodeRequest& request) {
    return normalized_scope_list(request);
}

std::string MicrosoftOAuthService::build_scope_string(const std::vector<std::string>& scopes) {
    std::string result;
    for (std::size_t index = 0; index < scopes.size(); ++index) {
        if (index > 0) {
            result.push_back(' ');
        }
        result += scopes[index];
    }
    return result;
}

bool MicrosoftOAuthService::is_retryable_error(const std::string& code) {
    return code == "authorization_pending" || code == "slow_down";
}

std::string MicrosoftOAuthService::describe_error(const std::string& code) {
    if (code == "authorization_pending") {
        return "The user has not completed Microsoft sign-in yet.";
    }
    if (code == "authorization_declined") {
        return "The user declined the device code sign-in request.";
    }
    if (code == "bad_verification_code") {
        return "The verification code was rejected by Microsoft.";
    }
    if (code == "expired_token") {
        return "The device code expired before sign-in completed.";
    }
    if (code == "slow_down") {
        return "Microsoft asked the client to poll less frequently.";
    }
    return "Unexpected Microsoft OAuth error: " + code;
}

DeviceCodeResponse MicrosoftOAuthService::parse_device_code_response(const HttpResponse& response) {
    DeviceCodeResponse result;
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        result.errorCode = "invalid_response";
        result.errorMessage = error_message_for_response(response);
        return result;
    }

    const auto& object = parsed.value.as_object();
    if (const auto* error = dawn::infra::json::find(object, "error"); error && error->is_string()) {
        result.errorCode = error->as_string();
        result.errorMessage = read_string(object, "error_description");
        if (result.errorMessage.empty()) {
            result.errorMessage = describe_error(result.errorCode);
        }
        return result;
    }

    result.deviceCode = read_string(object, "device_code");
    result.userCode = read_string(object, "user_code");
    result.verificationUri = read_string(object, "verification_uri");
    result.verificationUriComplete = read_string(object, "verification_uri_complete");
    result.expiresIn = read_int(object, "expires_in");
    result.interval = read_int(object, "interval", 5);
    result.message = read_string(object, "message");
    if (result.deviceCode.empty() || result.userCode.empty() || result.verificationUri.empty()) {
        result.errorCode = "invalid_response";
        result.errorMessage = "Microsoft device code response was missing required fields.";
        return result;
    }

    result.ok = true;
    return result;
}

TokenResponse MicrosoftOAuthService::parse_token_response(const HttpResponse& response) {
    TokenResponse result;
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        result.errorCode = "invalid_response";
        result.errorMessage = error_message_for_response(response);
        return result;
    }

    const auto& object = parsed.value.as_object();
    if (const auto* error = dawn::infra::json::find(object, "error"); error && error->is_string()) {
        result.errorCode = error->as_string();
        result.errorMessage = read_string(object, "error_description");
        if (result.errorMessage.empty()) {
            result.errorMessage = describe_error(result.errorCode);
        }
        result.retryable = is_retryable_error(result.errorCode);
        return result;
    }

    result.tokenType = read_string(object, "token_type");
    result.scope = read_string(object, "scope");
    result.expiresIn = read_int(object, "expires_in");
    result.accessToken = read_string(object, "access_token");
    result.refreshToken = read_string(object, "refresh_token");
    result.idToken = read_string(object, "id_token");
    if (result.accessToken.empty()) {
        result.errorCode = "invalid_response";
        result.errorMessage = "Microsoft token response was missing access_token.";
        return result;
    }

    result.ok = true;
    return result;
}

DeviceCodeResponse MicrosoftOAuthService::start_device_code_flow(const DeviceCodeRequest& request, std::string* error) const {
    if (!client_) {
        if (error) {
            *error = "missing http client";
        }
        return {};
    }
    if (request.clientId.empty()) {
        if (error) {
            *error = "client_id is required";
        }
        DeviceCodeResponse result;
        result.errorCode = "invalid_request";
        result.errorMessage = "client_id is required";
        return result;
    }

    const auto scopes = normalized_scopes(request);
    const auto body = dawn::infra::net::form_encode({
        {"client_id", request.clientId},
        {"scope", build_scope_string(scopes)},
    });

    auto httpRequest = make_request(HttpMethod::Post, device_code_url(request));
    httpRequest.body = body;
    const auto response = client_->send(httpRequest);
    if (!response.success()) {
        auto result = parse_device_code_response(response);
        if (error) {
            *error = result.errorMessage.empty() ? error_message_for_response(response) : result.errorMessage;
        }
        return result;
    }

    auto result = parse_device_code_response(response);
    if (!result.ok && error) {
        *error = result.errorMessage;
    }
    return result;
}

TokenResponse MicrosoftOAuthService::poll_token(const DeviceCodeRequest& request, const std::string& deviceCode, std::string* error) const {
    if (!client_) {
        if (error) {
            *error = "missing http client";
        }
        return {};
    }
    if (request.clientId.empty() || deviceCode.empty()) {
        if (error) {
            *error = "client_id and device_code are required";
        }
        TokenResponse result;
        result.errorCode = "invalid_request";
        result.errorMessage = "client_id and device_code are required";
        return result;
    }

    auto httpRequest = make_request(HttpMethod::Post, token_url(request));
    httpRequest.body = dawn::infra::net::form_encode({
        {"client_id", request.clientId},
        {"grant_type", "urn:ietf:params:oauth:grant-type:device_code"},
        {"device_code", deviceCode},
    });

    const auto response = client_->send(httpRequest);
    if (!response.success()) {
        auto result = parse_token_response(response);
        if (error) {
            *error = result.errorMessage.empty() ? error_message_for_response(response) : result.errorMessage;
        }
        return result;
    }

    auto result = parse_token_response(response);
    if (!result.ok && error) {
        *error = result.errorMessage;
    }
    return result;
}

TokenResponse MicrosoftOAuthService::refresh_token(const DeviceCodeRequest& request, const std::string& refreshToken, std::string* error) const {
    if (!client_) {
        if (error) {
            *error = "missing http client";
        }
        return {};
    }
    if (request.clientId.empty() || refreshToken.empty()) {
        if (error) {
            *error = "client_id and refresh_token are required";
        }
        TokenResponse result;
        result.errorCode = "invalid_request";
        result.errorMessage = "client_id and refresh_token are required";
        return result;
    }

    const auto scopes = normalized_scopes(request);
    auto httpRequest = make_request(HttpMethod::Post, token_url(request));
    httpRequest.body = dawn::infra::net::form_encode({
        {"client_id", request.clientId},
        {"grant_type", "refresh_token"},
        {"refresh_token", refreshToken},
        {"scope", build_scope_string(scopes)},
    });

    const auto response = client_->send(httpRequest);
    if (!response.success()) {
        auto result = parse_token_response(response);
        if (error) {
            *error = result.errorMessage.empty() ? error_message_for_response(response) : result.errorMessage;
        }
        return result;
    }

    auto result = parse_token_response(response);
    if (!result.ok && error) {
        *error = result.errorMessage;
    }
    return result;
}

DeviceFlowResult MicrosoftOAuthService::complete_device_flow(
    const DeviceCodeRequest& request,
    std::function<void(const std::string& userCode, const std::string& verificationUri)> callback,
    std::string* error) const {

    DeviceFlowResult result;

    // Step 1: Start device code flow
    auto deviceCodeResponse = start_device_code_flow(request, error);
    if (!deviceCodeResponse.ok) {
        result.errorMessage = deviceCodeResponse.errorMessage;
        return result;
    }

    // Notify caller to display user code and verification URI
    if (callback) {
        callback(deviceCodeResponse.userCode,
                 deviceCodeResponse.verificationUriComplete.empty()
                     ? deviceCodeResponse.verificationUri
                     : deviceCodeResponse.verificationUriComplete);
    }

    // Step 2: Poll for token
    const int expiresIn = deviceCodeResponse.expiresIn > 0 ? deviceCodeResponse.expiresIn : 900;
    const int interval = deviceCodeResponse.interval > 0 ? deviceCodeResponse.interval : 5;
    const auto startTime = std::chrono::steady_clock::now();

    while (true) {
        auto tokenResponse = poll_token(request, deviceCodeResponse.deviceCode, error);

        if (tokenResponse.ok) {
            result.ok = true;
            result.accessToken = tokenResponse.accessToken;
            result.refreshToken = tokenResponse.refreshToken;
            result.expiresIn = tokenResponse.expiresIn;
            return result;
        }

        // Check if error is retryable
        if (!tokenResponse.retryable) {
            result.errorMessage = tokenResponse.errorMessage;
            return result;
        }

        // Check if expired
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - startTime).count();
        if (elapsed >= expiresIn) {
            result.errorMessage = "Device code expired before authentication completed";
            return result;
        }

        // Wait before next poll
        std::this_thread::sleep_for(std::chrono::seconds(interval));
    }
}

DeviceCodeRequest MicrosoftOAuthService::create_minecraft_request() {
    DeviceCodeRequest request;
    request.tenant = "consumers";
    request.clientId = MINECRAFT_CLIENT_ID;
    request.scopes = {"XboxLive.signin", "offline_access"};
    return request;
}

} // namespace dawn::core
