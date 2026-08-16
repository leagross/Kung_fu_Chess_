#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <future>
#include <string>
#include <thread>
#include <vector>

#include "kfc/server/match_audience.hpp"

using kfc::model::PieceColor;
using kfc::server::MatchAudience;
using kfc::server::WatcherId;
using namespace std::chrono_literals;

namespace {

// Records what one connection was sent. Plain and unsynchronized: every test
// here either sends from the calling thread, or joins before asserting.
class Sink {
public:
    kfc::server::SendFn send_fn() {
        return [this](const std::string& text) { received.push_back(text); };
    }

    kfc::server::CloseFn close_fn() {
        return [this] { ++closes; };
    }

    std::vector<std::string> received;
    int closes = 0;
};

}  // namespace

// --- A watcher who leaves stops costing anything ---

TEST(MatchAudienceTest, AWatcherWhoLeftIsNoLongerBroadcastTo) {
    MatchAudience audience;
    Sink player, stays, leaves;
    ASSERT_TRUE(audience.seat("alice", player.send_fn(), player.close_fn()).has_value());
    WatcherId staying = audience.watch(stays.send_fn(), stays.close_fn());
    WatcherId leaving = audience.watch(leaves.send_fn(), leaves.close_fn());
    ASSERT_NE(staying, leaving) << "each watcher needs its own handle";
    ASSERT_EQ(audience.watcher_count(), 2u);

    audience.unwatch(leaving);
    audience.broadcast("update");

    EXPECT_EQ(audience.watcher_count(), 1u);
    EXPECT_EQ(player.received.size(), 1u);
    EXPECT_EQ(stays.received.size(), 1u);
    EXPECT_TRUE(leaves.received.empty()) << "a viewer who left must not still be sent to";
}

TEST(MatchAudienceTest, AWatcherWhoLeftIsNotClosedAgainWhenTheMatchEnds) {
    MatchAudience audience;
    Sink stays, leaves;
    (void)audience.watch(stays.send_fn(), stays.close_fn());
    WatcherId leaving = audience.watch(leaves.send_fn(), leaves.close_fn());

    audience.unwatch(leaving);
    audience.release_all();

    EXPECT_EQ(stays.closes, 1);
    EXPECT_EQ(leaves.closes, 0) << "closing a connection that already closed is not ours to do";
}

// A handle is spent once. If ids were reused -- or if unwatch matched on
// position rather than identity -- a close arriving late from a viewer who
// already left would silently unregister whoever took its place.
TEST(MatchAudienceTest, AStaleHandleCannotUnregisterTheWatcherThatCameAfterIt) {
    MatchAudience audience;
    Sink first, second;
    WatcherId gone = audience.watch(first.send_fn(), first.close_fn());
    audience.unwatch(gone);
    WatcherId newcomer = audience.watch(second.send_fn(), second.close_fn());
    ASSERT_NE(gone, newcomer);

    audience.unwatch(gone);  // the late close from the one that already left
    audience.broadcast("update");

    EXPECT_EQ(audience.watcher_count(), 1u);
    EXPECT_EQ(second.received.size(), 1u);
}

TEST(MatchAudienceTest, UnwatchingAHandleNobodyHoldsChangesNothing) {
    MatchAudience audience;
    Sink watcher;
    (void)audience.watch(watcher.send_fn(), watcher.close_fn());

    audience.unwatch(0);
    audience.unwatch(9999);
    audience.broadcast("update");

    EXPECT_EQ(audience.watcher_count(), 1u);
    EXPECT_EQ(watcher.received.size(), 1u);
}

// --- Nothing is ever sent while the table is locked ---

// The decisive test for the copy-on-write roster. A send is network I/O and can
// block for as long as a stalled socket keeps it blocked; if it ran under the
// table's mutex, every other connection's seating, watching and lookups would
// queue behind that one dead client. Here a send is held open deliberately and
// the rest of the table is exercised from another thread: with the lock held
// across sends, none of it would return and this would time out.
TEST(MatchAudienceTest, AStalledSendDoesNotBlockTheRestOfTheTable) {
    MatchAudience audience;
    std::promise<void> entered_send;
    std::atomic<bool> may_finish{false};
    Sink other;

    ASSERT_TRUE(audience
                    .seat("alice",
                          [&](const std::string&) {
                              entered_send.set_value();
                              while (!may_finish.load()) {
                                  std::this_thread::sleep_for(1ms);
                              }
                          },
                          {})
                    .has_value());

    std::thread broadcaster([&] { audience.broadcast("update"); });
    entered_send.get_future().wait();

    // Alice's send is now stuck, and will stay stuck until we say otherwise.
    std::future<bool> others = std::async(std::launch::async, [&] {
        WatcherId watcher = audience.watch(other.send_fn(), other.close_fn());
        audience.unwatch(watcher);
        return audience.username_of(PieceColor::White) == "alice";
    });

    std::future_status status = others.wait_for(5s);

    // Released before anything is asserted: if the table *was* locked, the other
    // thread is parked on that mutex, and every way out of this test -- joining
    // the broadcaster, waiting on the future, even destroying it -- would block
    // forever. A regression here has to fail, not hang.
    may_finish = true;
    broadcaster.join();

    ASSERT_EQ(status, std::future_status::ready) << "the table was locked for the whole of a send";
    EXPECT_TRUE(others.get());
}

// The same rule, for the reason it is not merely a slowdown: closing a socket
// comes back as a disconnect, and a viewer's disconnect calls unwatch. With the
// lock held across the closes that is a self-deadlock on a non-recursive mutex.
TEST(MatchAudienceTest, AWatcherMayUnregisterItselfFromInsideItsOwnClose) {
    MatchAudience audience;
    Sink player;
    ASSERT_TRUE(audience.seat("alice", player.send_fn(), player.close_fn()).has_value());

    WatcherId watcher = 0;
    int closes = 0;
    watcher = audience.watch([](const std::string&) {}, [&] {
        ++closes;
        audience.unwatch(watcher);  // exactly what the real disconnect path does
    });

    std::future<void> released = std::async(std::launch::async, [&] { audience.release_all(); });

    ASSERT_EQ(released.wait_for(5s), std::future_status::ready) << "release_all deadlocked on its own close";
    released.get();
    EXPECT_EQ(closes, 1);
    EXPECT_EQ(audience.watcher_count(), 0u);
}

// --- Seats behave as before ---

TEST(MatchAudienceTest, SeatsAreHandedOutWhiteThenBlackAndThenRefused) {
    MatchAudience audience;
    Sink white, black, third;

    EXPECT_EQ(audience.seat("alice", white.send_fn(), white.close_fn()), PieceColor::White);
    EXPECT_FALSE(audience.both_seats_taken());
    EXPECT_EQ(audience.seat("bob", black.send_fn(), black.close_fn()), PieceColor::Black);
    EXPECT_TRUE(audience.both_seats_taken());
    EXPECT_FALSE(audience.seat("carol", third.send_fn(), third.close_fn()).has_value());

    EXPECT_EQ(audience.username_of(PieceColor::White), "alice");
    EXPECT_EQ(audience.username_of(PieceColor::Black), "bob");
    EXPECT_TRUE(third.received.empty());
}

TEST(MatchAudienceTest, SendToReachesOneColourOnly) {
    MatchAudience audience;
    Sink white, black, watcher;
    ASSERT_TRUE(audience.seat("alice", white.send_fn(), white.close_fn()).has_value());
    ASSERT_TRUE(audience.seat("bob", black.send_fn(), black.close_fn()).has_value());
    (void)audience.watch(watcher.send_fn(), watcher.close_fn());

    audience.send_to(PieceColor::Black, "rejected");

    EXPECT_TRUE(white.received.empty());
    EXPECT_EQ(black.received.size(), 1u);
    EXPECT_TRUE(watcher.received.empty()) << "a rejection is nobody else's business";
}

// --- The watcher count is capped, unlike seats ---

TEST(MatchAudienceTest, TheSpectatorLimitRefusesAWatcherOnceReached) {
    MatchAudience audience;
    std::vector<Sink> sinks(MatchAudience::kMaxSpectators);
    for (Sink& sink : sinks) {
        ASSERT_NE(audience.watch(sink.send_fn(), sink.close_fn()), 0u);
    }
    ASSERT_EQ(audience.watcher_count(), MatchAudience::kMaxSpectators);

    Sink one_too_many;
    WatcherId refused = audience.watch(one_too_many.send_fn(), one_too_many.close_fn());

    EXPECT_EQ(refused, 0u) << "0 is the same sentinel unwatch() already treats as 'not a watcher'";
    EXPECT_EQ(audience.watcher_count(), MatchAudience::kMaxSpectators) << "the refused watcher must not be seated";
}

TEST(MatchAudienceTest, LeavingBelowTheLimitFreesUpASpotForTheNextWatcher) {
    MatchAudience audience;
    std::vector<Sink> sinks(MatchAudience::kMaxSpectators);
    std::vector<WatcherId> ids;
    for (Sink& sink : sinks) {
        ids.push_back(audience.watch(sink.send_fn(), sink.close_fn()));
    }

    audience.unwatch(ids.front());
    Sink newcomer;
    WatcherId admitted = audience.watch(newcomer.send_fn(), newcomer.close_fn());

    EXPECT_NE(admitted, 0u) << "a spot freed by unwatch() must be usable again, not permanently spent";
    EXPECT_EQ(audience.watcher_count(), MatchAudience::kMaxSpectators);
}

TEST(MatchAudienceTest, ReseatingKeepsTheUsernameAndRedirectsTheSends) {
    MatchAudience audience;
    Sink dropped, returned;
    ASSERT_TRUE(audience.seat("alice", dropped.send_fn(), dropped.close_fn()).has_value());

    audience.reseat(PieceColor::White, returned.send_fn(), returned.close_fn());
    audience.broadcast("update");

    EXPECT_EQ(audience.username_of(PieceColor::White), "alice") << "it is the same player, on a new socket";
    EXPECT_TRUE(dropped.received.empty());
    EXPECT_EQ(returned.received.size(), 1u);
}
