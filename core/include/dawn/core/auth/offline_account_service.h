#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <optional>

namespace dawn::core {

struct OfflineAccount {
    std::string id;
    std::string username;
    std::string uuid;
    std::string createdAt;
    std::string lastUsedAt;
};

struct OfflineAccountCreateRequest {
    std::string username;
};

struct OfflineAccountCreateResult {
    bool success = false;
    std::string accountId;
    std::string username;
    std::string uuid;
    std::string errorMessage;
};

struct OfflineAccountListResult {
    bool success = false;
    std::vector<OfflineAccount> accounts;
    std::string errorMessage;
};

class OfflineAccountService {
public:
    explicit OfflineAccountService(std::filesystem::path root);

    [[nodiscard]] const std::filesystem::path& root() const noexcept;
    [[nodiscard]] std::filesystem::path accounts_path() const;

    // Create a new offline account with generated UUID
    OfflineAccountCreateResult create_account(const OfflineAccountCreateRequest& request, std::string* error = nullptr);

    // Get all offline accounts
    [[nodiscard]] std::vector<OfflineAccount> list_accounts(std::string* error = nullptr) const;

    // Find account by ID
    [[nodiscard]] std::optional<OfflineAccount> find_account(const std::string& id, std::string* error = nullptr) const;

    // Find account by username
    [[nodiscard]] std::optional<OfflineAccount> find_account_by_username(const std::string& username, std::string* error = nullptr) const;

    // Check if username exists
    [[nodiscard]] bool username_exists(const std::string& username, std::string* error = nullptr) const;

    // Remove an offline account
    bool remove_account(const std::string& id, std::string* error = nullptr);

    // Update last used timestamp
    bool update_last_used(const std::string& id, std::string* error = nullptr);

    // Save accounts to disk
    bool save(std::string* error = nullptr) const;

    // Reload accounts from disk
    bool reload(std::string* error = nullptr);

    // Generate offline UUID from username (deterministic)
    [[nodiscard]] static std::string generate_offline_uuid(const std::string& username);

    // Validate username format
    [[nodiscard]] static bool is_valid_username(const std::string& username);

    // Get username validation error message
    [[nodiscard]] static std::string get_username_validation_error(const std::string& username);

private:
    [[nodiscard]] std::string make_account_id(const std::string& username) const;
    [[nodiscard]] std::string current_timestamp() const;

    std::filesystem::path root_;
    std::vector<OfflineAccount> accounts_;
};

} // namespace dawn::core
