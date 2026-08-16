#include <gtest/gtest.h>

#include <chrono>
#include <set>
#include <string>
#include <thread>
#include <vector>

#include "kfc/server/auth_token_store.hpp"

using kfc::server::AuthTokenStore;

TEST(AuthTokenStoreTest, IssuedTokenResolvesBackToItsUsername) {
    AuthTokenStore tokens;

    std::string token = tokens.issue("alice");

    ASSERT_TRUE(tokens.username_for(token).has_value());
    EXPECT_EQ(*tokens.username_for(token), "alice");
}

TEST(AuthTokenStoreTest, AGuessedOrNeverIssuedTokenResolvesToNothing) {
    AuthTokenStore tokens;
    tokens.issue("alice");

    EXPECT_FALSE(tokens.username_for("not-a-real-token").has_value());
    EXPECT_FALSE(tokens.username_for("").has_value());
}

TEST(AuthTokenStoreTest, TwoIssuesForTheSameUsernameProduceDifferentTokens) {
    AuthTokenStore tokens;

    std::string first = tokens.issue("alice");
    std::string second = tokens.issue("alice");

    EXPECT_NE(first, second);
    // Both stay valid -- see AuthTokenStore's own doc comment on why an
    // account can have more than one live token at once.
    EXPECT_EQ(*tokens.username_for(first), "alice");
    EXPECT_EQ(*tokens.username_for(second), "alice");
}

TEST(AuthTokenStoreTest, ConcurrentIssuesNeverCollideOrCorruptTheStore) {
    AuthTokenStore tokens;
    constexpr int kIssuers = 50;
    std::vector<std::string> issued(kIssuers);
    std::vector<std::thread> threads;
    threads.reserve(kIssuers);
    for (int i = 0; i < kIssuers; ++i) {
        threads.emplace_back([&tokens, &issued, i] { issued[i] = tokens.issue("user" + std::to_string(i)); });
    }
    for (std::thread& t : threads) {
        t.join();
    }

    std::set<std::string> unique_tokens(issued.begin(), issued.end());
    EXPECT_EQ(unique_tokens.size(), static_cast<std::size_t>(kIssuers)) << "two issuers collided on the same token";
    for (int i = 0; i < kIssuers; ++i) {
        ASSERT_TRUE(tokens.username_for(issued[i]).has_value());
        EXPECT_EQ(*tokens.username_for(issued[i]), "user" + std::to_string(i));
    }
}

TEST(AuthTokenStoreTest, ATokenIsStillValidRightBeforeItsLifetimeEnds) {
    AuthTokenStore tokens;
    auto issued_at = std::chrono::steady_clock::now();

    std::string token = tokens.issue("alice", issued_at);

    EXPECT_TRUE(tokens.username_for(token, issued_at + AuthTokenStore::kTokenLifetime - std::chrono::seconds(1))
                    .has_value());
}

TEST(AuthTokenStoreTest, ATokenStopsWorkingOnceItsLifetimeHasPassed) {
    AuthTokenStore tokens;
    auto issued_at = std::chrono::steady_clock::now();

    std::string token = tokens.issue("alice", issued_at);

    EXPECT_FALSE(
        tokens.username_for(token, issued_at + AuthTokenStore::kTokenLifetime + std::chrono::seconds(1)).has_value());
}

TEST(AuthTokenStoreTest, TwoTokensAreIndependentEitherOnesLifetimeDoesNotAffectTheOther) {
    AuthTokenStore tokens;
    auto issued_at = std::chrono::steady_clock::now();

    std::string first = tokens.issue("alice", issued_at);
    std::string second = tokens.issue("alice", issued_at + std::chrono::hours(1));

    auto after_first_expires = issued_at + AuthTokenStore::kTokenLifetime + std::chrono::seconds(1);
    EXPECT_FALSE(tokens.username_for(first, after_first_expires).has_value());
    EXPECT_TRUE(tokens.username_for(second, after_first_expires).has_value())
        << "issued an hour later, so still within its own lifetime";
}
