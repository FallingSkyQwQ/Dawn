#pragma once

#include "dawn/infra/net/http_client_factory.h"
#include "dawn/infra/net/http_client.h"
#include "dawn/infra/json/simple_json.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>

namespace dawn::core {

struct MicrosoftIdentityRequest {
    std::string oauthAccessToken;
    std::string xblRelyingParty = "http://auth.xboxlive.com";
    std::string xstsRelyingParty = "rp://api.minecraftservices.com/";
    std::string sandboxId = "RETAIL";
};

struct XblAuthenticateRequest {
    std::string oauthAccessToken;
    std::string relyingParty = "http://auth.xboxlive.com";
    std::string siteName = "user.auth.xboxlive.com";
};

struct XblAuthenticateResponse {
    bool ok = false;
    std::string token;
    std::string userHash;
    std::string issueInstant;
    std::string notAfter;
    std::string errorStage = "xbl";
    std::string errorCode;
    std::string errorMessage;
};

struct XstsAuthorizeRequest {
    std::string xblToken;
    std::string relyingParty = "rp://api.minecraftservices.com/";
    std::string sandboxId = "RETAIL";
};

struct XstsAuthorizeResponse {
    bool ok = false;
    std::string token;
    std::string userHash;
    std::string errorStage = "xsts";
    std::string errorCode;
    std::string errorMessage;
};

struct MinecraftAuthenticateRequest {
    std::string userHash;
    std::string xstsToken;
};

struct MinecraftAuthenticateResponse {
    bool ok = false;
    std::string tokenType;
    std::string accessToken;
    int expiresIn = 0;
    std::string errorStage = "minecraft_auth";
    std::string errorCode;
    std::string errorMessage;
};

struct MinecraftProfileRequest {
    std::string accessToken;
};

struct MinecraftProfileResponse {
    bool ok = false;
    std::string uuid;
    std::string displayName;
    std::string errorStage = "minecraft_profile";
    std::string errorCode;
    std::string errorMessage;
};

struct MicrosoftIdentityResult {
    bool ok = false;
    std::string oauthAccessToken;
    std::string xblToken;
    std::string xstsToken;
    std::string minecraftAccessToken;
    std::string userHash;
    std::string uuid;
    std::string displayName;
    std::string expiresAt;
    int expiresIn = 0;
    std::string errorStage;
    std::string errorCode;
    std::string errorMessage;
};

class MicrosoftIdentityService {
public:
    explicit MicrosoftIdentityService(std::shared_ptr<dawn::infra::net::HttpClient> client = {});

    [[nodiscard]] const std::shared_ptr<dawn::infra::net::HttpClient>& http_client() const noexcept;

    XblAuthenticateResponse authenticate_xbl(const XblAuthenticateRequest& request, std::string* error = nullptr) const;
    XstsAuthorizeResponse authorize_xsts(const XstsAuthorizeRequest& request, std::string* error = nullptr) const;
    MinecraftAuthenticateResponse authenticate_minecraft(const MinecraftAuthenticateRequest& request, std::string* error = nullptr) const;
    MinecraftProfileResponse fetch_minecraft_profile(const MinecraftProfileRequest& request, std::string* error = nullptr) const;
    MicrosoftIdentityResult authenticate(const MicrosoftIdentityRequest& request, std::string* error = nullptr, std::chrono::system_clock::time_point now = std::chrono::system_clock::now()) const;

    static std::string describe_error(const std::string& stage, const std::string& code);

private:
    [[nodiscard]] std::string xbl_url() const;
    [[nodiscard]] std::string xsts_url() const;
    [[nodiscard]] std::string minecraft_auth_url() const;
    [[nodiscard]] std::string minecraft_profile_url() const;
    [[nodiscard]] static std::string format_time_point(const std::chrono::system_clock::time_point& timePoint);

    [[nodiscard]] static XblAuthenticateResponse parse_xbl_response(const dawn::infra::net::HttpResponse& response);
    [[nodiscard]] static XstsAuthorizeResponse parse_xsts_response(const dawn::infra::net::HttpResponse& response);
    [[nodiscard]] static MinecraftAuthenticateResponse parse_minecraft_auth_response(const dawn::infra::net::HttpResponse& response);
    [[nodiscard]] static MinecraftProfileResponse parse_minecraft_profile_response(const dawn::infra::net::HttpResponse& response);
    [[nodiscard]] static std::string read_error_code(const dawn::infra::json::Value::Object& object);
    [[nodiscard]] static std::string read_error_message(const dawn::infra::json::Value::Object& object);
    [[nodiscard]] static std::string read_user_hash(const dawn::infra::json::Value::Object& object);
    [[nodiscard]] static dawn::infra::net::HttpRequest make_json_request(dawn::infra::net::HttpMethod method, const std::string& url, const dawn::infra::json::Value& body = dawn::infra::json::Value());
    [[nodiscard]] static MicrosoftIdentityResult fail_result(const std::string& stage, const std::string& code, const std::string& message);

    std::shared_ptr<dawn::infra::net::HttpClient> client_;
};

} // namespace dawn::core
