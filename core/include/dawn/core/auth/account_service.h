#pragma once

#include <filesystem>
#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace dawn::core {

enum class AccountType {
    Microsoft,
    Offline,
};

enum class TokenState {
    Unknown,
    Valid,
    Expired,
    Refreshing,
    Failed,
};

struct MicrosoftAccount {
    std::string accountId;
    std::string email;
    std::string displayName;
    std::string uuid;
    std::string accessToken;
    std::string refreshToken;
    std::string expiresAt;
    TokenState tokenState = TokenState::Unknown;
};

struct OfflineProfile {
    std::string profileId;
    std::string name;
    std::string uuid;
};

struct AccountProfile {
    std::string id;
    AccountType type = AccountType::Offline;
    MicrosoftAccount microsoft;
    OfflineProfile offline;
    TokenState tokenState = TokenState::Unknown;
    bool active = false;
};

struct AccountSwitchResult {
    bool success = false;
    std::string activeAccountId;
    std::string message;
};

TokenState update_account_token_state(AccountProfile* account, const std::chrono::system_clock::time_point& now = std::chrono::system_clock::now());
TokenState update_microsoft_token_state(MicrosoftAccount* account, const std::chrono::system_clock::time_point& now = std::chrono::system_clock::now());

class AccountService {
public:
    explicit AccountService(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] std::filesystem::path accounts_path() const;

    std::vector<AccountProfile> accounts(std::string* error = nullptr) const;
    std::optional<AccountProfile> active_account(std::string* error = nullptr) const;

    AccountProfile add_microsoft_account(MicrosoftAccount account, std::string* error = nullptr);
    AccountProfile add_offline_profile(OfflineProfile profile, std::string* error = nullptr);
    AccountSwitchResult activate_account(const std::string& id, std::string* error = nullptr);
    bool remove_account(const std::string& id, std::string* error = nullptr);
    bool save(std::string* error = nullptr) const;
    bool reload(std::string* error = nullptr);

private:
    [[nodiscard]] AccountProfile build_microsoft_profile(MicrosoftAccount account) const;
    [[nodiscard]] AccountProfile build_offline_profile(OfflineProfile profile) const;
    [[nodiscard]] std::string make_account_id(const std::string& seed) const;

    std::filesystem::path root_;
    std::vector<AccountProfile> accounts_;
    std::string activeAccountId_;
};

} // namespace dawn::core
