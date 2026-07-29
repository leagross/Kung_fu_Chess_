#include <gtest/gtest.h>

#include <atomic>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "kfc/server/session_registry.hpp"

using kfc::server::SessionRegistry;

TEST(SessionRegistryTest, AFreeNameCanBeClaimedAndATakenOneCannot) {
    SessionRegistry registry;

    std::optional<SessionRegistry::Lease> first = registry.claim("alice");
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->username(), "alice");
    EXPECT_EQ(registry.live_count(), 1u);

    EXPECT_FALSE(registry.claim("alice").has_value());
    EXPECT_EQ(registry.live_count(), 1u);
}

TEST(SessionRegistryTest, DifferentNamesDoNotCollide) {
    SessionRegistry registry;

    std::optional<SessionRegistry::Lease> alice = registry.claim("alice");
    std::optional<SessionRegistry::Lease> bob = registry.claim("bob");

    EXPECT_TRUE(alice.has_value());
    EXPECT_TRUE(bob.has_value());
    EXPECT_EQ(registry.live_count(), 2u);
}

TEST(SessionRegistryTest, DestroyingTheLeaseGivesTheNameBack) {
    SessionRegistry registry;
    {
        std::optional<SessionRegistry::Lease> lease = registry.claim("alice");
        ASSERT_TRUE(lease.has_value());
    }

    EXPECT_EQ(registry.live_count(), 0u);
    EXPECT_TRUE(registry.claim("alice").has_value()) << "the name was never given back";
}

// A moved-from lease must not release on destruction, or the name would be
// freed while its new owner is still using it -- letting a second connection in.
TEST(SessionRegistryTest, MovingALeaseMovesTheHoldRatherThanDuplicatingIt) {
    SessionRegistry registry;

    std::optional<SessionRegistry::Lease> original = registry.claim("alice");
    ASSERT_TRUE(original.has_value());
    {
        SessionRegistry::Lease moved = std::move(*original);
        original.reset();  // the husk goes away; the name must stay held
        EXPECT_EQ(registry.live_count(), 1u);
        EXPECT_FALSE(registry.claim("alice").has_value());
        EXPECT_EQ(moved.username(), "alice");
    }

    EXPECT_EQ(registry.live_count(), 0u) << "the real holder did not release on destruction";
}

TEST(SessionRegistryTest, MoveAssignmentReleasesWhateverTheTargetHeld) {
    SessionRegistry registry;

    std::optional<SessionRegistry::Lease> alice = registry.claim("alice");
    std::optional<SessionRegistry::Lease> bob = registry.claim("bob");
    ASSERT_TRUE(alice.has_value() && bob.has_value());

    *alice = std::move(*bob);  // alice's hold must be given up, not leaked

    EXPECT_EQ(registry.live_count(), 1u);
    EXPECT_TRUE(registry.claim("alice").has_value());
    EXPECT_EQ(alice->username(), "bob");
}

// Claiming has to be one locked test-and-insert. Written as a separate "is it
// free?" then "take it", two connections logging in as the same account at the
// same instant would both find it free and both proceed -- which is the exact
// thing this class exists to prevent.
TEST(SessionRegistryTest, SimultaneousClaimsForOneNameProduceExactlyOneWinner) {
    constexpr int kThreads = 8;
    constexpr int kRounds = 50;

    for (int round = 0; round < kRounds; ++round) {
        SessionRegistry registry;
        std::atomic<int> winners{0};
        std::atomic<int> attempts{0};
        std::atomic<bool> go{false};
        std::vector<std::thread> threads;

        for (int t = 0; t < kThreads; ++t) {
            threads.emplace_back([&] {
                while (!go.load()) {
                    std::this_thread::yield();  // line them all up on the same instant
                }
                std::optional<SessionRegistry::Lease> lease = registry.claim("alice");
                ++attempts;
                if (lease.has_value()) {
                    ++winners;
                    // The winner keeps the name until every thread has had its
                    // turn. Releasing sooner would let a later thread claim it
                    // perfectly legitimately, and this test would then be
                    // measuring its own timing rather than the registry.
                    while (attempts.load() < kThreads) {
                        std::this_thread::yield();
                    }
                }
            });
        }
        go = true;
        for (std::thread& thread : threads) {
            thread.join();
        }

        ASSERT_EQ(winners.load(), 1) << "two connections claimed the same account at once, in round " << round;
    }
}
