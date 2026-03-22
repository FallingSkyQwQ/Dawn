#include "dawn/core/auth/account_service.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;

bool parse_timestamp(const std::string& text, std::chrono::system_clock::time_point* out) {
    std::istringstream input(text);
    std::tm tm{};
    input >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    if (input.fail()) {
        return false;
    }

    const auto time = std::mktime(&tm);
    if (time == -1) {
        return false;
    }

    if (out) {
        *out = std::chrono::system_clock::from_time_t(time);
    }
    return true;
}

TokenState derive_microsoft_token_state(const MicrosoftAccount& account, const std::chrono::system_clock::time_point& now) {
    if (account.tokenState == TokenState::Failed) {
        return TokenState::Failed;
    }
    if (account.tokenState == TokenState::Refreshing) {
        return TokenState::Refreshing;
    }
    if (account.expiresAt.empty()) {
        return account.accessToken.empty() ? TokenState::Unknown : TokenState::Valid;
    }

    std::chrono::system_clock::time_point expiresAt;
    if (!parse_timestamp(account.expiresAt, &expiresAt)) {
        return account.accessToken.empty() ? TokenState::Unknown : TokenState::Valid;
    }
    return now >= expiresAt ? TokenState::Expired : TokenState::Valid;
}

std::string to_string(AccountType type) {
    switch (type) {
    case AccountType::Microsoft: return "microsoft";
    case AccountType::Offline: return "offline";
    }
    return "offline";
}

std::string to_string(TokenState state) {
    switch (state) {
    case TokenState::Unknown: return "unknown";
    case TokenState::Valid: return "valid";
    case TokenState::Expired: return "expired";
    case TokenState::Refreshing: return "refreshing";
    case TokenState::Failed: return "failed";
    }
    return "unknown";
}

AccountType account_type_from_string(const std::string& text) {
    if (text == "microsoft") {
        return AccountType::Microsoft;
    }
    return AccountType::Offline;
}

TokenState token_state_from_string(const std::string& text) {
    if (text == "valid") return TokenState::Valid;
    if (text == "expired") return TokenState::Expired;
    if (text == "refreshing") return TokenState::Refreshing;
    if (text == "failed") return TokenState::Failed;
    return TokenState::Unknown;
}

Value microsoft_to_json(const MicrosoftAccount& account) {
    Value::Object object;
    object.emplace("accountId", account.accountId);
    object.emplace("email", account.email);
    object.emplace("displayName", account.displayName);
    object.emplace("uuid", account.uuid);
    object.emplace("accessToken", account.accessToken);
    object.emplace("refreshToken", account.refreshToken);
    object.emplace("expiresAt", account.expiresAt);
    object.emplace("tokenState", to_string(account.tokenState));
    return Value(std::move(object));
}

Value offline_to_json(const OfflineProfile& profile) {
    Value::Object object;
    object.emplace("profileId", profile.profileId);
    object.emplace("name", profile.name);
    object.emplace("uuid", profile.uuid);
    return Value(std::move(object));
}

Value account_to_json(const AccountProfile& account) {
    Value::Object object;
    object.emplace("id", account.id);
    object.emplace("type", to_string(account.type));
    object.emplace("tokenState", to_string(account.tokenState));
    object.emplace("active", account.active);
    object.emplace("microsoft", microsoft_to_json(account.microsoft));
    object.emplace("offline", offline_to_json(account.offline));
    return Value(std::move(object));
}

AccountProfile account_from_json(const Value& value) {
    AccountProfile account;
    if (!value.is_object()) {
        return account;
    }

    const auto& object = value.as_object();
    if (const auto* id = dawn::infra::json::find(object, "id"); id && id->is_string()) account.id = id->as_string();
    if (const auto* type = dawn::infra::json::find(object, "type"); type && type->is_string()) account.type = account_type_from_string(type->as_string());
    if (const auto* token = dawn::infra::json::find(object, "tokenState"); token && token->is_string()) account.tokenState = token_state_from_string(token->as_string());
    if (const auto* active = dawn::infra::json::find(object, "active"); active && active->is_bool()) account.active = active->as_bool();

    if (const auto* microsoft = dawn::infra::json::find(object, "microsoft"); microsoft && microsoft->is_object()) {
        const auto& microsoft_object = microsoft->as_object();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "accountId"); value && value->is_string()) account.microsoft.accountId = value->as_string();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "email"); value && value->is_string()) account.microsoft.email = value->as_string();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "displayName"); value && value->is_string()) account.microsoft.displayName = value->as_string();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "uuid"); value && value->is_string()) account.microsoft.uuid = value->as_string();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "accessToken"); value && value->is_string()) account.microsoft.accessToken = value->as_string();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "refreshToken"); value && value->is_string()) account.microsoft.refreshToken = value->as_string();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "expiresAt"); value && value->is_string()) account.microsoft.expiresAt = value->as_string();
        if (const auto* value = dawn::infra::json::find(microsoft_object, "tokenState"); value && value->is_string()) account.microsoft.tokenState = token_state_from_string(value->as_string());
    }

    if (const auto* offline = dawn::infra::json::find(object, "offline"); offline && offline->is_object()) {
        const auto& offline_object = offline->as_object();
        if (const auto* value = dawn::infra::json::find(offline_object, "profileId"); value && value->is_string()) account.offline.profileId = value->as_string();
        if (const auto* value = dawn::infra::json::find(offline_object, "name"); value && value->is_string()) account.offline.name = value->as_string();
        if (const auto* value = dawn::infra::json::find(offline_object, "uuid"); value && value->is_string()) account.offline.uuid = value->as_string();
    }

    return account;
}

} // namespace

TokenState update_microsoft_token_state(MicrosoftAccount* account, const std::chrono::system_clock::time_point& now) {
    if (!account) {
        return TokenState::Unknown;
    }
    account->tokenState = derive_microsoft_token_state(*account, now);
    return account->tokenState;
}

TokenState update_account_token_state(AccountProfile* account, const std::chrono::system_clock::time_point& now) {
    if (!account) {
        return TokenState::Unknown;
    }

    if (account->type == AccountType::Microsoft) {
        account->tokenState = update_microsoft_token_state(&account->microsoft, now);
        return account->tokenState;
    }

    account->tokenState = TokenState::Valid;
    return account->tokenState;
}

AccountService::AccountService(std::filesystem::path root) : root_(std::move(root)) {
    reload(nullptr);
}

const std::filesystem::path& AccountService::root() const noexcept {
    return root_;
}

std::filesystem::path AccountService::accounts_path() const {
    return root_ / "accounts" / "accounts.json";
}

std::vector<AccountProfile> AccountService::accounts(std::string* error) const {
    if (error) {
        error->clear();
    }
    return accounts_;
}

std::optional<AccountProfile> AccountService::active_account(std::string* error) const {
    if (error) {
        error->clear();
    }
    const auto it = std::find_if(accounts_.begin(), accounts_.end(), [](const AccountProfile& account) {
        return account.active;
    });
    if (it == accounts_.end()) {
        return std::nullopt;
    }
    return *it;
}

AccountProfile AccountService::build_microsoft_profile(MicrosoftAccount account) const {
    AccountProfile profile;
    profile.id = account.accountId.empty() ? make_account_id(account.email) : account.accountId;
    profile.type = AccountType::Microsoft;
    profile.microsoft = std::move(account);
    profile.microsoft.accountId = profile.id;
    profile.tokenState = update_microsoft_token_state(&profile.microsoft);
    profile.active = accounts_.empty() || activeAccountId_.empty();
    if (profile.microsoft.displayName.empty()) {
        profile.microsoft.displayName = profile.microsoft.email.empty() ? profile.id : profile.microsoft.email;
    }
    return profile;
}

AccountProfile AccountService::build_offline_profile(OfflineProfile profile) const {
    AccountProfile account;
    account.id = profile.profileId.empty() ? make_account_id(profile.name) : profile.profileId;
    account.type = AccountType::Offline;
    account.offline = std::move(profile);
    account.offline.profileId = account.id;
    account.tokenState = TokenState::Valid;
    account.active = accounts_.empty() || activeAccountId_.empty();
    if (account.offline.name.empty()) {
        account.offline.name = account.id;
    }
    if (account.offline.uuid.empty()) {
        account.offline.uuid = account.id;
    }
    return account;
}

std::string AccountService::make_account_id(const std::string& seed) const {
    std::string id;
    id.reserve(seed.size() + 16);
    for (char ch : seed) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            id.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!id.empty() && id.back() != '-') {
            id.push_back('-');
        }
    }
    if (id.empty()) {
        id = "account";
    }
    id.push_back('-');
    id += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    return id;
}

AccountProfile AccountService::add_microsoft_account(MicrosoftAccount account, std::string* error) {
    auto profile = build_microsoft_profile(std::move(account));
    if (profile.active) {
        activeAccountId_ = profile.id;
    }
    accounts_.push_back(profile);
    save(error);
    return profile;
}

AccountProfile AccountService::add_offline_profile(OfflineProfile profile, std::string* error) {
    auto account = build_offline_profile(std::move(profile));
    if (account.active) {
        activeAccountId_ = account.id;
    }
    accounts_.push_back(account);
    save(error);
    return account;
}

AccountSwitchResult AccountService::activate_account(const std::string& id, std::string* error) {
    AccountSwitchResult result;
    bool found = false;
    for (auto& account : accounts_) {
        account.active = account.id == id;
        if (account.active) {
            found = true;
            activeAccountId_ = account.id;
        }
    }
    if (!found) {
        result.message = "account not found";
        if (error) {
            *error = result.message;
        }
        return result;
    }
    result.success = true;
    result.activeAccountId = activeAccountId_;
    result.message = "account activated";
    save(error);
    return result;
}

bool AccountService::remove_account(const std::string& id, std::string* error) {
    const auto it = std::remove_if(accounts_.begin(), accounts_.end(), [&](const AccountProfile& account) {
        return account.id == id;
    });
    if (it == accounts_.end()) {
        if (error) {
            *error = "account not found";
        }
        return false;
    }
    accounts_.erase(it, accounts_.end());
    if (!activeAccountId_.empty() && activeAccountId_ == id) {
        activeAccountId_.clear();
        if (!accounts_.empty()) {
            accounts_.front().active = true;
            activeAccountId_ = accounts_.front().id;
        }
    }
    return save(error);
}

bool AccountService::save(std::string* error) const {
    Value::Array array;
    for (const auto& account : accounts_) {
        array.emplace_back(account_to_json(account));
    }
    Value::Object object;
    object.emplace("activeAccountId", activeAccountId_);
    object.emplace("accounts", Value(std::move(array)));
    return dawn::infra::fs::write_text_file(accounts_path(), dawn::infra::json::stringify(Value(std::move(object)), 2), error);
}

bool AccountService::reload(std::string* error) {
    std::string text;
    accounts_.clear();
    activeAccountId_.clear();
    if (!std::filesystem::exists(accounts_path())) {
        if (error) {
            error->clear();
        }
        return true;
    }
    if (!dawn::infra::fs::read_text_file(accounts_path(), &text, error)) {
        return false;
    }

    const auto parsed = dawn::infra::json::parse(text);
    if (!parsed.ok) {
        if (error) {
            *error = parsed.error.message;
        }
        return false;
    }
    if (!parsed.value.is_object()) {
        return false;
    }

    const auto& object = parsed.value.as_object();
    if (const auto* active = dawn::infra::json::find(object, "activeAccountId"); active && active->is_string()) {
        activeAccountId_ = active->as_string();
    }

    const auto* accounts = dawn::infra::json::find(object, "accounts");
    if (accounts && accounts->is_array()) {
        for (const auto& entry : accounts->as_array()) {
            accounts_.push_back(account_from_json(entry));
            update_account_token_state(&accounts_.back());
        }
    }

    if (!activeAccountId_.empty()) {
        for (auto& account : accounts_) {
            account.active = account.id == activeAccountId_;
        }
    } else if (!accounts_.empty()) {
        accounts_.front().active = true;
        activeAccountId_ = accounts_.front().id;
    }
    return true;
}

} // namespace dawn::core
