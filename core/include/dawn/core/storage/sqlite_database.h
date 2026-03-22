#pragma once

#include <filesystem>
#include <string>

namespace dawn::core {

class SqliteDatabase {
public:
    explicit SqliteDatabase(std::filesystem::path path);

    [[nodiscard]] const std::filesystem::path& path() const noexcept;
    [[nodiscard]] bool is_available() const noexcept;
    [[nodiscard]] std::string last_error() const;

    bool open(std::string* error = nullptr);
    bool execute_schema(std::string* error = nullptr);

private:
    std::filesystem::path path_;
    bool available_ = false;
    std::string last_error_;
};

} // namespace dawn::core
