#include <gtest/gtest.h>

#include <chrono>
#include <string>

#include "kfc/server/rate_limiter.hpp"

using kfc::server::RateLimiter;

namespace {
// A fixed reference point, not steady_clock::now(): everything here moves
// time forward from here explicitly, so nothing depends on how fast the test
// itself runs. Deliberately far from time_point{} (a fresh Bucket's default
// window_start) -- using the same zero value here would make "a brand-new
// key's first call always starts a fresh window" pass by coincidence rather
// than by actually exercising that reset.
std::chrono::steady_clock::time_point start() {
    return std::chrono::steady_clock::time_point{} + std::chrono::hours(1000);
}
}  // namespace

TEST(RateLimiterTest, AllowsUpToMaxAttemptsWithinOneWindow) {
    RateLimiter limiter(3, std::chrono::seconds(60));
    std::chrono::steady_clock::time_point now = start();

    EXPECT_TRUE(limiter.allow("1.2.3.4", now));
    EXPECT_TRUE(limiter.allow("1.2.3.4", now));
    EXPECT_TRUE(limiter.allow("1.2.3.4", now));
}

TEST(RateLimiterTest, RefusesTheAttemptAfterMaxWithinTheSameWindow) {
    RateLimiter limiter(3, std::chrono::seconds(60));
    std::chrono::steady_clock::time_point now = start();

    limiter.allow("1.2.3.4", now);
    limiter.allow("1.2.3.4", now);
    limiter.allow("1.2.3.4", now);

    EXPECT_FALSE(limiter.allow("1.2.3.4", now)) << "the 4th attempt in the window should be refused";
    // Still refused, not just once -- a caller that keeps retrying does not
    // get to slip one through before the window actually rolls over.
    EXPECT_FALSE(limiter.allow("1.2.3.4", now + std::chrono::seconds(30)));
}

TEST(RateLimiterTest, AllowsAgainOnceTheWindowRollsOver) {
    RateLimiter limiter(2, std::chrono::seconds(60));
    std::chrono::steady_clock::time_point now = start();

    limiter.allow("1.2.3.4", now);
    limiter.allow("1.2.3.4", now);
    ASSERT_FALSE(limiter.allow("1.2.3.4", now)) << "budget should be used up";

    EXPECT_TRUE(limiter.allow("1.2.3.4", now + std::chrono::seconds(60)))
        << "a full window later, the budget should have reset";
}

TEST(RateLimiterTest, DifferentKeysHaveIndependentBudgets) {
    RateLimiter limiter(1, std::chrono::seconds(60));
    std::chrono::steady_clock::time_point now = start();

    EXPECT_TRUE(limiter.allow("1.2.3.4", now));
    EXPECT_FALSE(limiter.allow("1.2.3.4", now)) << "1.2.3.4's own budget is used up";
    EXPECT_TRUE(limiter.allow("5.6.7.8", now)) << "a different key must not share 1.2.3.4's budget";
}
