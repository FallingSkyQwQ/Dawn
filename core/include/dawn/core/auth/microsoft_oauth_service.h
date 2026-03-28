#pragma once

#include "dawn/infra/net/http_client_factory.h"
#include "dawn/infra/net/http_client.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace dawn::core {

struct DeviceCodeRequest {
    std::string tenant = "consumers";  // Use "consumers" for personal Microsoft accounts
    std::string clientId;
    std::vector<std::string> scopes;
};

struct DeviceCodeResponse {
    bool ok = false;
    std::string deviceCode;
    std::string userCode;
    std::string verificationUri;
    std::string verificationUriComplete;
    int expiresIn = 0;
    int interval = 5;
    std::string message;
    std::string errorCode;
    std::string errorMessage;
};

struct TokenResponse {
    bool ok = false;
    bool retryable = false;
    std::string tokenType;
    std::string scope;
    int expiresIn = 0;
    std::string accessToken;
    std::string refreshToken;
    std::string idToken;
    std::string errorCode;
    std::string errorMessage;
};

struct DeviceFlowResult {
    bool ok = false;
    std::string accessToken;
    std::string refreshToken;
    int expiresIn = 0;
    std::string errorMessage;
};

class MicrosoftOAuthService {
public:
    explicit MicrosoftOAuthService(std::shared_ptr<dawn::infra::net::HttpClient> client = {});

    [[nodiscard]] const std::shared_ptr<dawn::infra::net::HttpClient>& http_client() const noexcept;

    // Device Code Flow - Step 1: Request device code
    DeviceCodeResponse start_device_code_flow(const DeviceCodeRequest& request, std::string* error = nullptr) const;

    // Device Code Flow - Step 2: Poll for token
    TokenResponse poll_token(const DeviceCodeRequest& request, const std::string& deviceCode, std::string* error = nullptr) const;

    // Refresh token
    TokenResponse refresh_token(const DeviceCodeRequest& request, const std::string& refreshToken, std::string* error = nullptr) const;

    // Complete device flow with polling and callbacks
    // callback will be called with (userCode, verificationUri) for user display
    // Returns final token result
    DeviceFlowResult complete_device_flow(
        const DeviceCodeRequest& request,
        std::function<void(const std::string& userCode, const std::string& verificationUri)> callback,
        std::string* error = nullptr) const;

    // Create default Minecraft OAuth request
    [[nodiscard]] static DeviceCodeRequest create_minecraft_request();

    static std::string describe_error(const std::string& code);

private:
    [[nodiscard]] std::string tenant(const DeviceCodeRequest& request) const;
    [[nodiscard]] std::string device_code_url(const DeviceCodeRequest& request) const;
    [[nodiscard]] std::string token_url(const DeviceCodeRequest& request) const;
    [[nodiscard]] static std::vector<std::string> normalized_scopes(const DeviceCodeRequest& request);
    [[nodiscard]] static DeviceCodeResponse parse_device_code_response(const dawn::infra::net::HttpResponse& response);
    [[nodiscard]] static TokenResponse parse_token_response(const dawn::infra::net::HttpResponse& response);
    [[nodiscard]] static std::string build_scope_string(const std::vector<std::string>& scopes);
    [[nodiscard]] static bool is_retryable_error(const std::string& code);

    std::shared_ptr<dawn::infra::net::HttpClient> client_;
};

} // namespace dawn::core
