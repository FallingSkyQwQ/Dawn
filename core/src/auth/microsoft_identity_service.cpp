#include "dawn/core/auth/microsoft_identity_service.h"

#include "dawn/infra/json/simple_json.h"

#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;
using dawn::infra::net::HttpMethod;
using dawn::infra::net::HttpRequest;
using dawn::infra::net::HttpResponse;

constexpr const char* kXblUrl = "https://user.auth.xboxlive.com/user/authenticate";
constexpr const char* kXstsUrl = "https://xsts.auth.xboxlive.com/xsts/authorize";
constexpr const char* kMinecraftAuthUrl = "https://api.minecraftservices.com/authentication/login_with_xbox";
constexpr const char* kMinecraftProfileUrl = "https://api.minecraftservices.com/minecraft/profile";

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

std::string read_number_as_string(const Value::Object& object, const std::string& key) {
    const auto* value = dawn::infra::json::find(object, key);
    if (value && value->is_number()) {
        return std::to_string(static_cast<long long>(value->as_number()));
    }
    return {};
}

std::string read_user_hash_impl(const Value::Object& object) {
    const auto* displayClaims = dawn::infra::json::find(object, "DisplayClaims");
    if (displayClaims && displayClaims->is_object()) {
        const auto* xui = dawn::infra::json::find(displayClaims->as_object(), "xui");
        if (xui && xui->is_array() && !xui->as_array().empty()) {
            const auto& first = xui->as_array().front();
            if (first.is_object()) {
                return read_string(first.as_object(), "uhs");
            }
        }
    }

    const auto* xui = dawn::infra::json::find(object, "xui");
    if (xui && xui->is_array() && !xui->as_array().empty()) {
        const auto& first = xui->as_array().front();
        if (first.is_object()) {
            return read_string(first.as_object(), "uhs");
        }
    }
    return {};
}

std::string error_from_response(const std::string& stage, const HttpResponse& response) {
    const auto parsed = dawn::infra::json::parse(response.body);
    if (parsed.ok && parsed.value.is_object()) {
        const auto& object = parsed.value.as_object();
        std::string code = read_string(object, "error");
        if (code.empty()) {
            code = read_number_as_string(object, "XErr");
        }
        std::string message = read_string(object, "error_description");
        if (message.empty()) {
            message = read_string(object, "Message");
        }
        if (message.empty()) {
            message = read_string(object, "message");
        }
        if (!code.empty()) {
            if (message.empty()) {
                message = MicrosoftIdentityService::describe_error(stage, code);
            }
            return code + ": " + message;
        }
    }

    std::ostringstream out;
    out << stage << " failed with HTTP " << response.statusCode;
    return out.str();
}

HttpRequest make_get_request(const std::string& url) {
    HttpRequest request;
    request.method = HttpMethod::Get;
    request.url = url;
    request.headers.emplace("Accept", "application/json");
    return request;
}

Value make_xbl_body(const XblAuthenticateRequest& request) {
    Value::Object properties;
    properties.emplace("AuthMethod", "RPS");
    properties.emplace("SiteName", request.siteName);
    properties.emplace("RpsTicket", std::string("d=") + request.oauthAccessToken);

    Value::Object body;
    body.emplace("Properties", Value(std::move(properties)));
    body.emplace("RelyingParty", request.relyingParty);
    body.emplace("TokenType", "JWT");
    return Value(std::move(body));
}

Value make_xsts_body(const XstsAuthorizeRequest& request) {
    Value::Object properties;
    properties.emplace("SandboxId", request.sandboxId);
    Value::Array userTokens;
    userTokens.emplace_back(request.xblToken);
    properties.emplace("UserTokens", Value(std::move(userTokens)));

    Value::Object body;
    body.emplace("Properties", Value(std::move(properties)));
    body.emplace("RelyingParty", request.relyingParty);
    body.emplace("TokenType", "JWT");
    return Value(std::move(body));
}

Value make_minecraft_body(const MinecraftAuthenticateRequest& request) {
    Value::Object body;
    body.emplace("identityToken", std::string("XBL3.0 x=") + request.userHash + ";" + request.xstsToken);
    return Value(std::move(body));
}

MicrosoftIdentityResult make_result_failure(const std::string& stage, const std::string& code, const std::string& message) {
    MicrosoftIdentityResult result;
    result.ok = false;
    result.errorStage = stage;
    result.errorCode = code;
    result.errorMessage = message.empty() ? MicrosoftIdentityService::describe_error(stage, code) : message;
    return result;
}

} // namespace

MicrosoftIdentityService::MicrosoftIdentityService(std::shared_ptr<dawn::infra::net::HttpClient> client)
    : client_(client ? std::move(client) : dawn::infra::net::HttpClientFactory::create_default_http_client()) {}

const std::shared_ptr<dawn::infra::net::HttpClient>& MicrosoftIdentityService::http_client() const noexcept {
    return client_;
}

std::string MicrosoftIdentityService::xbl_url() const {
    return kXblUrl;
}

std::string MicrosoftIdentityService::xsts_url() const {
    return kXstsUrl;
}

std::string MicrosoftIdentityService::minecraft_auth_url() const {
    return kMinecraftAuthUrl;
}

std::string MicrosoftIdentityService::minecraft_profile_url() const {
    return kMinecraftProfileUrl;
}

std::string MicrosoftIdentityService::format_time_point(const std::chrono::system_clock::time_point& timePoint) {
    const auto time = std::chrono::system_clock::to_time_t(timePoint);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream out;
    out << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return out.str();
}

std::string MicrosoftIdentityService::read_error_code(const Value::Object& object) {
    if (const auto* value = dawn::infra::json::find(object, "error"); value && value->is_string()) {
        return value->as_string();
    }
    if (const auto* value = dawn::infra::json::find(object, "XErr"); value && value->is_number()) {
        return std::to_string(static_cast<long long>(value->as_number()));
    }
    if (const auto* value = dawn::infra::json::find(object, "code"); value && value->is_string()) {
        return value->as_string();
    }
    return {};
}

std::string MicrosoftIdentityService::read_error_message(const Value::Object& object) {
    if (const auto* value = dawn::infra::json::find(object, "error_description"); value && value->is_string()) {
        return value->as_string();
    }
    if (const auto* value = dawn::infra::json::find(object, "Message"); value && value->is_string()) {
        return value->as_string();
    }
    if (const auto* value = dawn::infra::json::find(object, "message"); value && value->is_string()) {
        return value->as_string();
    }
    if (const auto* value = dawn::infra::json::find(object, "errorMessage"); value && value->is_string()) {
        return value->as_string();
    }
    return {};
}

std::string MicrosoftIdentityService::read_user_hash(const Value::Object& object) {
    return read_user_hash_impl(object);
}

HttpRequest MicrosoftIdentityService::make_json_request(HttpMethod method, const std::string& url, const Value& body) {
    HttpRequest request;
    request.method = method;
    request.url = url;
    request.headers.emplace("Accept", "application/json");
    if (method == HttpMethod::Post || method == HttpMethod::Put || method == HttpMethod::Patch) {
        request.headers.emplace("Content-Type", "application/json");
        request.body = dawn::infra::json::stringify(body, 0);
    }
    return request;
}

MicrosoftIdentityResult MicrosoftIdentityService::fail_result(const std::string& stage, const std::string& code, const std::string& message) {
    return make_result_failure(stage, code, message);
}

std::string MicrosoftIdentityService::describe_error(const std::string& stage, const std::string& code) {
    if (code == "invalid_request") {
        return "The Microsoft identity request is missing required fields.";
    }
    if (code == "invalid_response") {
        return "The Microsoft identity endpoint returned an invalid payload.";
    }
    if (stage == "xbl") {
        if (code == "2148916233") {
            return "The account does not have an Xbox profile.";
        }
        if (code == "2148916235") {
            return "The account is not allowed to use Xbox Live.";
        }
        return "Xbox Live authentication failed: " + code;
    }
    if (stage == "xsts") {
        if (code == "2148916233") {
            return "The XSTS service rejected the Xbox account or sandbox.";
        }
        if (code == "2148916236") {
            return "The account is restricted from XSTS authorization.";
        }
        return "XSTS authorization failed: " + code;
    }
    if (stage == "minecraft_auth") {
        if (code == "unauthorized") {
            return "Minecraft rejected the Xbox identity token.";
        }
        return "Minecraft authentication failed: " + code;
    }
    if (stage == "minecraft_profile") {
        if (code == "forbidden" || code == "unauthorized") {
            return "Minecraft profile lookup was rejected.";
        }
        return "Minecraft profile lookup failed: " + code;
    }
    return stage + " failed: " + code;
}

XblAuthenticateResponse MicrosoftIdentityService::parse_xbl_response(const HttpResponse& response) {
    XblAuthenticateResponse result;
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        result.errorCode = "invalid_response";
        result.errorMessage = error_from_response(result.errorStage, response);
        return result;
    }

    const auto& object = parsed.value.as_object();
    result.errorCode = read_error_code(object);
    if (!result.errorCode.empty()) {
        result.errorMessage = read_error_message(object);
        if (result.errorMessage.empty()) {
            result.errorMessage = describe_error(result.errorStage, result.errorCode);
        }
        return result;
    }

    result.token = read_string(object, "Token");
    result.issueInstant = read_string(object, "IssueInstant");
    result.notAfter = read_string(object, "NotAfter");
    result.userHash = read_user_hash(object);
    if (result.token.empty() || result.userHash.empty()) {
        result.errorCode = "invalid_response";
        result.errorMessage = "Xbox Live authenticate response was missing Token or user hash.";
        return result;
    }

    result.ok = true;
    return result;
}

XstsAuthorizeResponse MicrosoftIdentityService::parse_xsts_response(const HttpResponse& response) {
    XstsAuthorizeResponse result;
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        result.errorCode = "invalid_response";
        result.errorMessage = error_from_response(result.errorStage, response);
        return result;
    }

    const auto& object = parsed.value.as_object();
    result.errorCode = read_error_code(object);
    if (!result.errorCode.empty()) {
        result.errorMessage = read_error_message(object);
        if (result.errorMessage.empty()) {
            result.errorMessage = describe_error(result.errorStage, result.errorCode);
        }
        return result;
    }

    result.token = read_string(object, "Token");
    result.userHash = read_user_hash(object);
    if (result.token.empty() || result.userHash.empty()) {
        result.errorCode = "invalid_response";
        result.errorMessage = "XSTS authorize response was missing Token or user hash.";
        return result;
    }

    result.ok = true;
    return result;
}

MinecraftAuthenticateResponse MicrosoftIdentityService::parse_minecraft_auth_response(const HttpResponse& response) {
    MinecraftAuthenticateResponse result;
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        result.errorCode = "invalid_response";
        result.errorMessage = error_from_response(result.errorStage, response);
        return result;
    }

    const auto& object = parsed.value.as_object();
    result.errorCode = read_error_code(object);
    if (!result.errorCode.empty()) {
        result.errorMessage = read_error_message(object);
        if (result.errorMessage.empty()) {
            result.errorMessage = describe_error(result.errorStage, result.errorCode);
        }
        return result;
    }

    result.tokenType = read_string(object, "token_type");
    result.accessToken = read_string(object, "access_token");
    result.expiresIn = read_int(object, "expires_in");
    if (result.accessToken.empty() || result.expiresIn <= 0) {
        result.errorCode = "invalid_response";
        result.errorMessage = "Minecraft authenticate response was missing access_token or expires_in.";
        return result;
    }

    result.ok = true;
    return result;
}

MinecraftProfileResponse MicrosoftIdentityService::parse_minecraft_profile_response(const HttpResponse& response) {
    MinecraftProfileResponse result;
    const auto parsed = dawn::infra::json::parse(response.body);
    if (!parsed.ok || !parsed.value.is_object()) {
        result.errorCode = "invalid_response";
        result.errorMessage = error_from_response(result.errorStage, response);
        return result;
    }

    const auto& object = parsed.value.as_object();
    result.errorCode = read_error_code(object);
    if (!result.errorCode.empty()) {
        result.errorMessage = read_error_message(object);
        if (result.errorMessage.empty()) {
            result.errorMessage = describe_error(result.errorStage, result.errorCode);
        }
        return result;
    }

    result.uuid = read_string(object, "id");
    result.displayName = read_string(object, "name");
    if (result.uuid.empty() || result.displayName.empty()) {
        result.errorCode = "invalid_response";
        result.errorMessage = "Minecraft profile response was missing id or name.";
        return result;
    }

    result.ok = true;
    return result;
}

XblAuthenticateResponse MicrosoftIdentityService::authenticate_xbl(const XblAuthenticateRequest& request, std::string* error) const {
    if (!client_) {
        if (error) {
            *error = "missing http client";
        }
        XblAuthenticateResponse result;
        result.errorCode = "missing_http_client";
        result.errorMessage = "missing http client";
        return result;
    }
    if (request.oauthAccessToken.empty()) {
        if (error) {
            *error = "oauth access token is required";
        }
        XblAuthenticateResponse result;
        result.errorCode = "invalid_request";
        result.errorMessage = "oauth access token is required";
        return result;
    }

    const auto response = client_->send(make_json_request(HttpMethod::Post, xbl_url(), make_xbl_body(request)));
    auto result = parse_xbl_response(response);
    if (!result.ok && error) {
        *error = result.errorMessage;
    }
    return result;
}

XstsAuthorizeResponse MicrosoftIdentityService::authorize_xsts(const XstsAuthorizeRequest& request, std::string* error) const {
    if (!client_) {
        if (error) {
            *error = "missing http client";
        }
        XstsAuthorizeResponse result;
        result.errorCode = "missing_http_client";
        result.errorMessage = "missing http client";
        return result;
    }
    if (request.xblToken.empty()) {
        if (error) {
            *error = "xbl token is required";
        }
        XstsAuthorizeResponse result;
        result.errorCode = "invalid_request";
        result.errorMessage = "xbl token is required";
        return result;
    }

    const auto response = client_->send(make_json_request(HttpMethod::Post, xsts_url(), make_xsts_body(request)));
    auto result = parse_xsts_response(response);
    if (!result.ok && error) {
        *error = result.errorMessage;
    }
    return result;
}

MinecraftAuthenticateResponse MicrosoftIdentityService::authenticate_minecraft(const MinecraftAuthenticateRequest& request, std::string* error) const {
    if (!client_) {
        if (error) {
            *error = "missing http client";
        }
        MinecraftAuthenticateResponse result;
        result.errorCode = "missing_http_client";
        result.errorMessage = "missing http client";
        return result;
    }
    if (request.userHash.empty() || request.xstsToken.empty()) {
        if (error) {
            *error = "user hash and xsts token are required";
        }
        MinecraftAuthenticateResponse result;
        result.errorCode = "invalid_request";
        result.errorMessage = "user hash and xsts token are required";
        return result;
    }

    const auto response = client_->send(make_json_request(HttpMethod::Post, minecraft_auth_url(), make_minecraft_body(request)));
    auto result = parse_minecraft_auth_response(response);
    if (!result.ok && error) {
        *error = result.errorMessage;
    }
    return result;
}

MinecraftProfileResponse MicrosoftIdentityService::fetch_minecraft_profile(const MinecraftProfileRequest& request, std::string* error) const {
    if (!client_) {
        if (error) {
            *error = "missing http client";
        }
        MinecraftProfileResponse result;
        result.errorCode = "missing_http_client";
        result.errorMessage = "missing http client";
        return result;
    }
    if (request.accessToken.empty()) {
        if (error) {
            *error = "minecraft access token is required";
        }
        MinecraftProfileResponse result;
        result.errorCode = "invalid_request";
        result.errorMessage = "minecraft access token is required";
        return result;
    }

    auto httpRequest = make_get_request(minecraft_profile_url());
    httpRequest.headers.emplace("Authorization", std::string("Bearer ") + request.accessToken);
    const auto response = client_->send(httpRequest);
    auto result = parse_minecraft_profile_response(response);
    if (!result.ok && error) {
        *error = result.errorMessage;
    }
    return result;
}

MicrosoftIdentityResult MicrosoftIdentityService::authenticate(const MicrosoftIdentityRequest& request, std::string* error, std::chrono::system_clock::time_point now) const {
    if (request.oauthAccessToken.empty()) {
        if (error) {
            *error = "OAuth access token is required.";
        }
        return fail_result("oauth", "invalid_request", "OAuth access token is required.");
    }

    std::string stage_error;
    const auto xbl = authenticate_xbl(XblAuthenticateRequest{request.oauthAccessToken, request.xblRelyingParty}, &stage_error);
    if (!xbl.ok) {
        if (error) {
            *error = stage_error.empty() ? xbl.errorMessage : stage_error;
        }
        return fail_result(xbl.errorStage, xbl.errorCode, stage_error.empty() ? xbl.errorMessage : stage_error);
    }

    const auto xsts = authorize_xsts(XstsAuthorizeRequest{xbl.token, request.xstsRelyingParty, request.sandboxId}, &stage_error);
    if (!xsts.ok) {
        if (error) {
            *error = stage_error.empty() ? xsts.errorMessage : stage_error;
        }
        return fail_result(xsts.errorStage, xsts.errorCode, stage_error.empty() ? xsts.errorMessage : stage_error);
    }

    const auto minecraft = authenticate_minecraft(MinecraftAuthenticateRequest{xsts.userHash.empty() ? xbl.userHash : xsts.userHash, xsts.token}, &stage_error);
    if (!minecraft.ok) {
        if (error) {
            *error = stage_error.empty() ? minecraft.errorMessage : stage_error;
        }
        return fail_result(minecraft.errorStage, minecraft.errorCode, stage_error.empty() ? minecraft.errorMessage : stage_error);
    }

    const auto profile = fetch_minecraft_profile(MinecraftProfileRequest{minecraft.accessToken}, &stage_error);
    if (!profile.ok) {
        if (error) {
            *error = stage_error.empty() ? profile.errorMessage : stage_error;
        }
        return fail_result(profile.errorStage, profile.errorCode, stage_error.empty() ? profile.errorMessage : stage_error);
    }

    MicrosoftIdentityResult result;
    result.ok = true;
    result.oauthAccessToken = request.oauthAccessToken;
    result.xblToken = xbl.token;
    result.xstsToken = xsts.token;
    result.userHash = !xsts.userHash.empty() ? xsts.userHash : xbl.userHash;
    result.minecraftAccessToken = minecraft.accessToken;
    result.uuid = profile.uuid;
    result.displayName = profile.displayName;
    result.expiresIn = minecraft.expiresIn;
    result.expiresAt = format_time_point(now + std::chrono::seconds(minecraft.expiresIn));
    return result;
}

} // namespace dawn::core
