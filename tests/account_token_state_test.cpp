#include "dawn/core/auth/account_service.h"

#include <gtest/gtest.h>

#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

using namespace dawn::core;

namespace {

std::string format_time(const std::chrono::system_clock::time_point& timePoint) {
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

} // namespace

TEST(AccountTokenState, MarksExpiredMicrosoftTokens) {
    MicrosoftAccount account;
    account.accessToken = "access";
    account.expiresAt = format_time(std::chrono::system_clock::now() - std::chrono::hours(1));

    const auto state = update_microsoft_token_state(&account);
    EXPECT_EQ(state, TokenState::Expired);
    EXPECT_EQ(account.tokenState, TokenState::Expired);
}

