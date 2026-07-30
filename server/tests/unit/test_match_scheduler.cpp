#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "kfc/io/board_parser.hpp"
#include "kfc/model/board.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/json.hpp"
#include "kfc/server/match.hpp"
#include "kfc/server/match_scheduler.hpp"

using namespace kfc::model;
using namespace kfc::protocol;
using kfc::server::Match;
using kfc::server::MatchScheduler;

namespace {

// Same fixture idea as test_match/test_room_manager: one pawn each side, with
// room to move, so a real move produces a real BoardUpdate.
Board make_two_pawn_board() {
    return kfc::io::BoardParser().parse({
        ". . .",
        ". . .",
        "wP . bP",
    });
}

// Thread-safe recorder of everything a match sent to one connection -- the
// scheduler's worker thread broadcasts, this test's assertions run on the
// main thread.
class RecordingSink {
public:
    kfc::server::SendFn as_send_fn() {
        return [this](const std::string& text) {
            std::lock_guard<std::mutex> guard(mutex_);
            messages_.push_back(text);
        };
    }

    bool wait_for(int timeout_ms, const std::function<bool(const ServerMessage&)>& predicate) const {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                for (const std::string& text : messages_) {
                    std::optional<ServerMessage> decoded = decode_server_message(text);
                    if (decoded.has_value() && predicate(*decoded)) {
                        return true;
                    }
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> messages_;
};

bool is_board_update(const ServerMessage& message) {
    return std::holds_alternative<BoardUpdate>(message);
}

std::filesystem::path log_path() {
    return std::filesystem::temp_directory_path() / "kfc_match_scheduler_test.log";
}

}  // namespace

TEST(MatchSchedulerTest, WorkerCountDefaultsToAtLeastOne) {
    MatchScheduler scheduler;
    EXPECT_GE(scheduler.worker_count(), 1u);
}

TEST(MatchSchedulerTest, WorkerCountHonorsAnExplicitValue) {
    MatchScheduler scheduler(3);
    EXPECT_EQ(scheduler.worker_count(), 3u);
}

TEST(MatchSchedulerTest, AddAndRemoveTrackMatchCount) {
    FileLogger logger(log_path());
    MatchScheduler scheduler(1);
    EXPECT_EQ(scheduler.match_count(), 0u);

    auto match = std::make_shared<Match>(make_two_pawn_board(), logger);
    scheduler.add(match);
    EXPECT_EQ(scheduler.match_count(), 1u);

    scheduler.remove(match);
    EXPECT_EQ(scheduler.match_count(), 0u);
}

TEST(MatchSchedulerTest, AddIgnoresANullMatch) {
    MatchScheduler scheduler(1);
    scheduler.add(nullptr);
    EXPECT_EQ(scheduler.match_count(), 0u);
}

TEST(MatchSchedulerTest, RemoveOfAnUnknownMatchIsHarmless) {
    FileLogger logger(log_path());
    MatchScheduler scheduler(1);
    auto match = std::make_shared<Match>(make_two_pawn_board(), logger);
    // Never added -- remove() must be a no-op, not a crash.
    scheduler.remove(match);
    EXPECT_EQ(scheduler.match_count(), 0u);
}

// The whole point of MatchScheduler: a Match with no thread of its own gets
// ticked purely because it is registered here -- nothing in this test ever
// calls Match::tick() by hand.
TEST(MatchSchedulerTest, AnAddedMatchIsActuallyTickedByAWorker) {
    FileLogger logger(log_path());
    auto match = std::make_shared<Match>(make_two_pawn_board(), logger);

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_TRUE(match->join("white", white_sink.as_send_fn()).has_value());
    ASSERT_TRUE(match->join("black", black_sink.as_send_fn()).has_value());

    MatchScheduler scheduler(1);
    scheduler.add(match);

    match->enqueue(PieceColor::White, ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});

    EXPECT_TRUE(white_sink.wait_for(1000, is_board_update));

    scheduler.remove(match);
}

// remove() unregisters immediately and never joins a thread (see
// match_scheduler.hpp) -- it must return long before the scheduler's own
// destructor (which does join every worker) would.
TEST(MatchSchedulerTest, RemoveReturnsWithoutWaitingForAWorker) {
    FileLogger logger(log_path());
    MatchScheduler scheduler(1);
    auto match = std::make_shared<Match>(make_two_pawn_board(), logger);
    scheduler.add(match);

    auto started = std::chrono::steady_clock::now();
    scheduler.remove(match);
    auto elapsed = std::chrono::steady_clock::now() - started;

    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(), 50);
}
