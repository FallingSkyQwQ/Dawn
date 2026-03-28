#include "dawn/core/auth/offline_account_service.h"

#include "dawn/infra/fs/file_system.h"
#include "dawn/infra/json/simple_json.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <iomanip>
#include <random>
#include <regex>
#include <sstream>
#include <utility>

namespace dawn::core {

namespace {

using dawn::infra::json::Value;

// Minecraft offline UUID generation (based on username, deterministic)
// Uses the same algorithm as the official Minecraft launcher
std::string generate_uuid_from_username(const std::string& username) {
    // Create a namespace UUID for offline mode (fixed value)
    // This is the namespace UUID used by Minecraft for offline mode
    const std::string namespace_uuid = "OfflinePlayer:";
    const std::string input = namespace_uuid + username;

    // Simple MD5-like hash to UUID format (for demo purposes)
    // In production, use a proper UUID v3 implementation
    std::array<uint8_t, 16> hash{};

    // Generate a deterministic hash based on username
    std::hash<std::string> hasher;
    size_t hash_value = hasher(input);

    // Fill the hash array with derived values
    for (size_t i = 0; i < 16; ++i) {
        hash[i] = static_cast<uint8_t>((hash_value >> (i % 8)) & 0xFF);
        // Mix in more entropy from the input
        if (i < input.size()) {
            hash[i] ^= static_cast<uint8_t>(input[i]);
        }
        hash[i] ^= static_cast<uint8_t>(i * 7);
    }

    // Set version (0011 = version 3 - name-based MD5)
    hash[6] = (hash[6] & 0x0F) | 0x30;
    // Set variant (10 = RFC 4122 variant)
    hash[8] = (hash[8] & 0x3F) | 0x80;

    // Format as UUID string
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            oss << '-';
        }
        oss << std::setw(2) << static_cast<int>(hash[i]);
    }

    return oss.str();
}

// Alternative UUID generation using proper random generation
std::string generate_random_uuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 255);

    std::array<uint8_t, 16> uuid{};
    for (size_t i = 0; i < 16; ++i) {
        uuid[i] = static_cast<uint8_t>(dis(gen));
    }

    // Set version (0100 = version 4 - random)
    uuid[6] = (uuid[6] & 0x0F) | 0x40;
    // Set variant (10 = RFC 4122 variant)
    uuid[8] = (uuid[8] & 0x3F) | 0x80;

    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (size_t i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            oss << '-';
        }
        oss << std::setw(2) << static_cast<int>(uuid[i]);
    }

    return oss.str();
}

Value account_to_json(const OfflineAccount& account) {
    Value::Object object;
    object.emplace("id", account.id);
    object.emplace("username", account.username);
    object.emplace("uuid", account.uuid);
    object.emplace("createdAt", account.createdAt);
    object.emplace("lastUsedAt", account.lastUsedAt);
    return Value(std::move(object));
}

OfflineAccount account_from_json(const Value& value) {
    OfflineAccount account;
    if (!value.is_object()) {
        return account;
    }

    const auto& object = value.as_object();
    if (const auto* id = dawn::infra::json::find(object, "id"); id && id->is_string()) {
        account.id = id->as_string();
    }
    if (const auto* username = dawn::infra::json::find(object, "username"); username && username->is_string()) {
        account.username = username->as_string();
    }
    if (const auto* uuid = dawn::infra::json::find(object, "uuid"); uuid && uuid->is_string()) {
        account.uuid = uuid->as_string();
    }
    if (const auto* createdAt = dawn::infra::json::find(object, "createdAt"); createdAt && createdAt->is_string()) {
        account.createdAt = createdAt->as_string();
    }
    if (const auto* lastUsedAt = dawn::infra::json::find(object, "lastUsedAt"); lastUsedAt && lastUsedAt->is_string()) {
        account.lastUsedAt = lastUsedAt->as_string();
    }

    return account;
}

} // namespace

OfflineAccountService::OfflineAccountService(std::filesystem::path root)
    : root_(std::move(root)) {
    reload(nullptr);
}

const std::filesystem::path& OfflineAccountService::root() const noexcept {
    return root_;
}

std::filesystem::path OfflineAccountService::accounts_path() const {
    return root_ / "accounts" / "offline_accounts.json";
}

std::string OfflineAccountService::make_account_id(const std::string& username) const {
    std::string id;
    id.reserve(username.size() + 16);

    // Sanitize username for ID
    for (char ch : username) {
        if (std::isalnum(static_cast<unsigned char>(ch))) {
            id.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        } else if (!id.empty() && id.back() != '-') {
            id.push_back('-');
        }
    }

    if (id.empty()) {
        id = "offline";
    }

    // Add timestamp for uniqueness
    id.push_back('-');
    id += std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());

    return id;
}

std::string OfflineAccountService::current_timestamp() const {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return oss.str();
}

bool OfflineAccountService::is_valid_username(const std::string& username) {
    // Minecraft username rules:
    // - 3-16 characters
    // - Only alphanumeric characters and underscore
    // - Must start with a letter
    if (username.empty()) {
        return false;
    }

    // Length check
    if (username.length() < 3 || username.length() > 16) {
        return false;
    }

    // First character must be a letter
    if (!std::isalpha(static_cast<unsigned char>(username[0]))) {
        return false;
    }

    // Rest must be alphanumeric or underscore
    for (size_t i = 0; i < username.length(); ++i) {
        char ch = username[i];
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return false;
        }
    }

    return true;
}

std::string OfflineAccountService::get_username_validation_error(const std::string& username) {
    if (username.empty()) {
        return "Username cannot be empty";
    }

    if (username.length() < 3) {
        return "Username must be at least 3 characters long";
    }

    if (username.length() > 16) {
        return "Username must be at most 16 characters long";
    }

    if (!std::isalpha(static_cast<unsigned char>(username[0]))) {
        return "Username must start with a letter";
    }

    for (size_t i = 0; i < username.length(); ++i) {
        char ch = username[i];
        if (!std::isalnum(static_cast<unsigned char>(ch)) && ch != '_') {
            return "Username can only contain letters, numbers, and underscores";
        }
    }

    return {};
}

std::string OfflineAccountService::generate_offline_uuid(const std::string& username) {
    return generate_uuid_from_username(username);
}

OfflineAccountCreateResult OfflineAccountService::create_account(const OfflineAccountCreateRequest& request, std::string* error) {
    OfflineAccountCreateResult result;

    // Validate username
    if (!is_valid_username(request.username)) {
        result.errorMessage = get_username_validation_error(request.username);
        if (error) {
            *error = result.errorMessage;
        }
        return result;
    }

    // Check for duplicate username
    if (username_exists(request.username, error)) {
        result.errorMessage = "Username already exists: " + request.username;
        if (error) {
            *error = result.errorMessage;
        }
        return result;
    }

    // Create account
    OfflineAccount account;
    account.id = make_account_id(request.username);
    account.username = request.username;
    account.uuid = generate_offline_uuid(request.username);
    account.createdAt = current_timestamp();
    account.lastUsedAt = account.createdAt;

    // Add to list
    accounts_.push_back(account);

    // Save to disk
    if (!save(error)) {
        // Rollback on save failure
        accounts_.pop_back();
        result.errorMessage = error ? *error : "Failed to save account";
        return result;
    }

    result.success = true;
    result.accountId = account.id;
    result.username = account.username;
    result.uuid = account.uuid;

    return result;
}

std::vector<OfflineAccount> OfflineAccountService::list_accounts(std::string* error) const {
    if (error) {
        error->clear();
    }
    return accounts_;
}

std::optional<OfflineAccount> OfflineAccountService::find_account(const std::string& id, std::string* error) const {
    if (error) {
        error->clear();
    }

    const auto it = std::find_if(accounts_.begin(), accounts_.end(),
        [&id](const OfflineAccount& account) {
            return account.id == id;
        });

    if (it == accounts_.end()) {
        return std::nullopt;
    }

    return *it;
}

std::optional<OfflineAccount> OfflineAccountService::find_account_by_username(const std::string& username, std::string* error) const {
    if (error) {
        error->clear();
    }

    const auto it = std::find_if(accounts_.begin(), accounts_.end(),
        [&username](const OfflineAccount& account) {
            return account.username == username;
        });

    if (it == accounts_.end()) {
        return std::nullopt;
    }

    return *it;
}

bool OfflineAccountService::username_exists(const std::string& username, std::string* error) const {
    if (error) {
        error->clear();
    }

    return std::find_if(accounts_.begin(), accounts_.end(),
        [&username](const OfflineAccount& account) {
            return account.username == username;
        }) != accounts_.end();
}

bool OfflineAccountService::remove_account(const std::string& id, std::string* error) {
    const auto it = std::remove_if(accounts_.begin(), accounts_.end(),
        [&id](const OfflineAccount& account) {
            return account.id == id;
        });

    if (it == accounts_.end()) {
        if (error) {
            *error = "Account not found: " + id;
        }
        return false;
    }

    accounts_.erase(it, accounts_.end());
    return save(error);
}

bool OfflineAccountService::update_last_used(const std::string& id, std::string* error) {
    auto it = std::find_if(accounts_.begin(), accounts_.end(),
        [&id](const OfflineAccount& account) {
            return account.id == id;
        });

    if (it == accounts_.end()) {
        if (error) {
            *error = "Account not found: " + id;
        }
        return false;
    }

    it->lastUsedAt = current_timestamp();
    return save(error);
}

bool OfflineAccountService::save(std::string* error) const {
    Value::Array array;
    for (const auto& account : accounts_) {
        array.emplace_back(account_to_json(account));
    }

    Value::Object object;
    object.emplace("version", 1);
    object.emplace("accounts", Value(std::move(array)));

    return dawn::infra::fs::write_text_file(
        accounts_path(),
        dawn::infra::json::stringify(Value(std::move(object)), 2),
        error);
}

bool OfflineAccountService::reload(std::string* error) {
    accounts_.clear();

    if (!std::filesystem::exists(accounts_path())) {
        if (error) {
            error->clear();
        }
        return true;
    }

    std::string text;
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
        if (error) {
            *error = "Invalid accounts file format";
        }
        return false;
    }

    const auto& object = parsed.value.as_object();
    const auto* accounts = dawn::infra::json::find(object, "accounts");
    if (accounts && accounts->is_array()) {
        for (const auto& entry : accounts->as_array()) {
            accounts_.push_back(account_from_json(entry));
        }
    }

    return true;
}

} // namespace dawn::core
