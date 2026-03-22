#include "dawn/core/storage/sqlite_database.h"

#include <utility>

#if defined(DAWN_HAS_SQLITE3)
#include <sqlite3.h>
#endif

namespace dawn::core {

namespace {

constexpr const char* kSchema = R"sql(
CREATE TABLE IF NOT EXISTS instances (
    id TEXT PRIMARY KEY,
    manifest_json TEXT NOT NULL
);

CREATE TABLE IF NOT EXISTS task_history (
    id TEXT PRIMARY KEY,
    result_json TEXT NOT NULL
);
)sql";

} // namespace

SqliteDatabase::SqliteDatabase(std::filesystem::path path) : path_(std::move(path)) {}

const std::filesystem::path& SqliteDatabase::path() const noexcept {
    return path_;
}

bool SqliteDatabase::is_available() const noexcept {
    return available_;
}

std::string SqliteDatabase::last_error() const {
    return last_error_;
}

bool SqliteDatabase::open(std::string* error) {
#if defined(DAWN_HAS_SQLITE3)
    sqlite3* handle = nullptr;
    const int status = sqlite3_open_v2(path_.string().c_str(), &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (status != SQLITE_OK) {
        last_error_ = handle ? sqlite3_errmsg(handle) : "failed to open sqlite database";
        if (handle) {
            sqlite3_close(handle);
        }
        available_ = false;
        if (error) {
            *error = last_error_;
        }
        return false;
    }
    sqlite3_close(handle);
    available_ = true;
    last_error_.clear();
    return true;
#else
    last_error_ = "sqlite support is disabled at build time";
    available_ = false;
    if (error) {
        *error = last_error_;
    }
    return false;
#endif
}

bool SqliteDatabase::execute_schema(std::string* error) {
#if defined(DAWN_HAS_SQLITE3)
    sqlite3* handle = nullptr;
    const int status = sqlite3_open_v2(path_.string().c_str(), &handle, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr);
    if (status != SQLITE_OK) {
        last_error_ = handle ? sqlite3_errmsg(handle) : "failed to open sqlite database";
        if (handle) {
            sqlite3_close(handle);
        }
        if (error) {
            *error = last_error_;
        }
        return false;
    }

    char* sqlite_error = nullptr;
    const int exec_status = sqlite3_exec(handle, kSchema, nullptr, nullptr, &sqlite_error);
    if (exec_status != SQLITE_OK) {
        last_error_ = sqlite_error ? sqlite_error : "failed to execute schema";
        sqlite3_free(sqlite_error);
        sqlite3_close(handle);
        if (error) {
            *error = last_error_;
        }
        return false;
    }

    sqlite3_close(handle);
    last_error_.clear();
    return true;
#else
    last_error_ = "sqlite support is disabled at build time";
    if (error) {
        *error = last_error_;
    }
    return false;
#endif
}

} // namespace dawn::core
