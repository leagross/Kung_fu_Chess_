#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <mutex>
#include <thread>
#include <vector>

#include "kfc/io/board_parser.hpp"
#include "kfc/protocol/json.hpp"
#include "kfc/server/match.hpp"

using namespace kfc::model;
using namespace kfc::protocol;

namespace {

// A tiny board with one white pawn and one black pawn, positioned with
// genuine room to move forward (white moves toward row 0, black toward the
// last row -- see PawnRule) -- keeps every test focused purely on the
// ownership check, not on chess legality.
Board make_two_pawn_board() {
    return kfc::io::BoardParser().parse({
        ". . .",
        ". . .",
        "wP . bP",
    });
}

// Collects every message Match sends to one connection, thread-safely --
// Match::broadcast/send_to run on its own tick thread, this test's
// assertions run on the main thread.
class RecordingSink {
public:
    kfc::server::SendFn as_send_fn() {
        return [this](const std::string& text) {
            std::lock_guard<std::mutex> guard(mutex_);
            messages_.push_back(text);
        };
    }

    std::vector<std::string> snapshot() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return messages_;
    }

    // Polls up to timeout for at least one received message whose decoded
    // type matches predicate -- Match's tick thread runs on a real ~16ms
    // cadence, so this test observes it in real (short) wall-clock time
    // rather than pretending it can fake that clock away.
    bool wait_for(int timeout_ms, const std::function<bool(const ServerMessage&)>& predicate) const {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            for (const std::string& text : snapshot()) {
                std::optional<ServerMessage> decoded = decode_server_message(text);
                if (decoded.has_value() && predicate(*decoded)) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    // The CloseFn side of the same connection: records that Match released this
    // player, and lets a test wait for it the same way it waits for a message.
    kfc::server::CloseFn as_close_fn() {
        return [this]() {
            std::lock_guard<std::mutex> guard(mutex_);
            closed_ = true;
        };
    }

    bool wait_for_close(int timeout_ms) const {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
        while (std::chrono::steady_clock::now() < deadline) {
            {
                std::lock_guard<std::mutex> guard(mutex_);
                if (closed_) {
                    return true;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }

    bool is_closed() const {
        std::lock_guard<std::mutex> guard(mutex_);
        return closed_;
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::string> messages_;
    bool closed_ = false;
};

bool is_move_rejected_with(const ServerMessage& message, const std::string& reason) {
    return std::holds_alternative<MoveRejected>(message) && std::get<MoveRejected>(message).reason == reason;
}

bool is_board_update(const ServerMessage& message) {
    return std::holds_alternative<BoardUpdate>(message);
}

bool is_game_over_won_by(const ServerMessage& message, PieceColor winner) {
    return std::holds_alternative<GameOver>(message) && std::get<GameOver>(message).winner == winner;
}

}  // namespace

TEST(MatchOwnershipTest, RejectsAMoveRequestForAPieceOfTheWrongColor) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_ownership_test.log");
    kfc::server::Match match(make_two_pawn_board(), logger);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn()), PieceColor::Black);

    // White (alice) tries to move Black's pawn at (2,2).
    match.enqueue(PieceColor::White, ClientMessage{MoveRequest{Position{2, 2}, Position{1, 2}}});

    EXPECT_TRUE(white_sink.wait_for(500, [](const ServerMessage& m) {
        return is_move_rejected_with(m, "not_your_piece");
    }));
    EXPECT_FALSE(black_sink.wait_for(200, is_board_update));

    match.stop();
}

TEST(MatchOwnershipTest, AcceptsAMoveRequestForOnesOwnPiece) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_ownership_test.log");
    kfc::server::Match match(make_two_pawn_board(), logger);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn()), PieceColor::Black);

    // White (alice) moves her own pawn at (2,0) one step forward.
    match.enqueue(PieceColor::White, ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});

    EXPECT_TRUE(white_sink.wait_for(500, is_board_update));
    EXPECT_FALSE(white_sink.wait_for(200, [](const ServerMessage& m) {
        return std::holds_alternative<MoveRejected>(m);
    }));

    match.stop();
}

TEST(MatchJoinTest, MatchStartIsSentToBothOnlyWhenTheSecondPlayerJoins) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_join_test.log");
    kfc::server::Match match(make_two_pawn_board(), logger);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;

    // First player in: seated, but no MatchStart yet (still waiting).
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn()), PieceColor::White);
    EXPECT_FALSE(white_sink.wait_for(200, [](const ServerMessage& m) {
        return std::holds_alternative<MatchStart>(m);
    }));

    // Second player in: both are told the match can begin.
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn()), PieceColor::Black);
    EXPECT_TRUE(white_sink.wait_for(500, [](const ServerMessage& m) {
        return std::holds_alternative<MatchStart>(m);
    }));
    EXPECT_TRUE(black_sink.wait_for(500, [](const ServerMessage& m) {
        return std::holds_alternative<MatchStart>(m);
    }));

    match.stop();
}

TEST(MatchResignTest, ResignAwardsTheWinToTheOpponentAndTellsBothPlayers) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_resign_test.log");
    kfc::server::Match match(make_two_pawn_board(), logger);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn()), PieceColor::Black);

    // White (alice) resigns -> Black wins, and both players are told.
    match.enqueue(PieceColor::White, ClientMessage{Resign{}});

    EXPECT_TRUE(white_sink.wait_for(500, [](const ServerMessage& m) {
        return is_game_over_won_by(m, PieceColor::Black);
    }));
    EXPECT_TRUE(black_sink.wait_for(500, [](const ServerMessage& m) {
        return is_game_over_won_by(m, PieceColor::Black);
    }));

    match.stop();
}

TEST(MatchDisconnectTest, ShowsACountdownThenForfeitsAfterTheGrace) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_disconnect_test.log");
    // 150ms grace so the countdown resolves fast.
    kfc::server::Match match(make_two_pawn_board(), logger, {}, {}, /*disconnect_grace_ms=*/150);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn()), PieceColor::Black);

    // Black (bob) drops. White (still connected) first sees the countdown...
    match.on_disconnect(PieceColor::Black);
    EXPECT_TRUE(white_sink.wait_for(200, [](const ServerMessage& m) {
        return std::holds_alternative<OpponentDisconnected>(m);
    }));
    // ...then, once the grace runs out, White wins by forfeit.
    EXPECT_TRUE(white_sink.wait_for(1000, [](const ServerMessage& m) {
        return is_game_over_won_by(m, PieceColor::White);
    }));

    match.stop();
}

TEST(MatchReleaseTest, ForfeitAfterTheCountdownReleasesBothPlayers) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_release_test.log");
    // Short grace and short release delay so the whole sequence resolves fast.
    kfc::server::Match match(make_two_pawn_board(), logger, {}, {}, /*disconnect_grace_ms=*/100,
                             /*release_delay_ms=*/50);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn(), white_sink.as_close_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn(), black_sink.as_close_fn()), PieceColor::Black);

    // Black drops; once the grace runs out White wins by forfeit -- and then
    // *both* connections are let go, so nobody is left sitting in a room whose
    // game is already decided.
    match.on_disconnect(PieceColor::Black);
    ASSERT_TRUE(white_sink.wait_for(1000, [](const ServerMessage& m) {
        return is_game_over_won_by(m, PieceColor::White);
    }));

    EXPECT_TRUE(white_sink.wait_for_close(1000));
    EXPECT_TRUE(black_sink.wait_for_close(1000));

    match.stop();
}

TEST(MatchReleaseTest, PlayersAreNotReleasedWhileTheGameIsStillOn) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_release_test.log");
    kfc::server::Match match(make_two_pawn_board(), logger, {}, {}, /*disconnect_grace_ms=*/20000,
                             /*release_delay_ms=*/20);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn(), white_sink.as_close_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn(), black_sink.as_close_fn()), PieceColor::Black);

    // Well past the (deliberately tiny) release delay, but the match is still
    // undecided -- the release is triggered by game-over, never by mere time.
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(white_sink.is_closed());
    EXPECT_FALSE(black_sink.is_closed());

    match.stop();
}

TEST(MatchReleaseTest, ANormalWinAlsoReleasesBothPlayers) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_release_test.log");
    kfc::server::Match match(make_two_pawn_board(), logger, {}, {}, /*disconnect_grace_ms=*/20000,
                             /*release_delay_ms=*/50);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn(), white_sink.as_close_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn(), black_sink.as_close_fn()), PieceColor::Black);

    // A decided game is a finished room too, however it was decided.
    match.enqueue(PieceColor::White, ClientMessage{Resign{}});
    ASSERT_TRUE(black_sink.wait_for(1000, [](const ServerMessage& m) {
        return is_game_over_won_by(m, PieceColor::Black);
    }));

    EXPECT_TRUE(white_sink.wait_for_close(1000));
    EXPECT_TRUE(black_sink.wait_for_close(1000));

    match.stop();
}

TEST(MatchDisconnectTest, ForfeitReportsTheDisconnectReasonToTheResultHook) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_disconnect_test.log");

    std::mutex result_mutex;
    bool fired = false;
    kfc::server::GameEndReason reason = kfc::server::GameEndReason::Draw;
    std::optional<PieceColor> winner;
    auto on_result = [&](kfc::server::GameEndReason r, std::optional<PieceColor> w, const std::string&,
                         const std::string&) {
        std::lock_guard<std::mutex> guard(result_mutex);
        reason = r;
        winner = w;
        fired = true;
    };

    kfc::server::Match match(make_two_pawn_board(), logger, {}, on_result, /*disconnect_grace_ms=*/120);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn()), PieceColor::Black);

    match.on_disconnect(PieceColor::White);  // white drops -> black should win by forfeit

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(1000);
    bool ok = false;
    while (std::chrono::steady_clock::now() < deadline) {
        {
            std::lock_guard<std::mutex> guard(result_mutex);
            if (fired) {
                ok = true;
                break;
            }
        }
        // Slept with the lock *released* -- note the inner scope above. on_result
        // runs on the match's own tick thread and needs this very mutex to
        // record the result; holding it across the sleep leaves it taken
        // essentially all the time, and on a platform whose mutexes let a
        // re-locking thread barge ahead of a waiting one (glibc does; Windows
        // SRW locks happen not to) the tick thread never gets in and the result
        // never lands, however long the timeout is.
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    ASSERT_TRUE(ok);
    std::lock_guard<std::mutex> guard(result_mutex);
    EXPECT_EQ(reason, kfc::server::GameEndReason::Disconnect);
    ASSERT_TRUE(winner.has_value());
    EXPECT_EQ(*winner, PieceColor::Black);

    match.stop();
}

TEST(MatchResignTest, CommandsArrivingAfterTheGameIsOverAreIgnored) {
    FileLogger logger(std::filesystem::temp_directory_path() / "kfc_match_resign_test.log");
    kfc::server::Match match(make_two_pawn_board(), logger);
    match.start();

    RecordingSink white_sink;
    RecordingSink black_sink;
    ASSERT_EQ(match.join("alice", white_sink.as_send_fn()), PieceColor::White);
    ASSERT_EQ(match.join("bob", black_sink.as_send_fn()), PieceColor::Black);

    match.enqueue(PieceColor::White, ClientMessage{Resign{}});
    ASSERT_TRUE(black_sink.wait_for(500, [](const ServerMessage& m) {
        return is_game_over_won_by(m, PieceColor::Black);
    }));

    // A move that would ordinarily be accepted arrives after the resign --
    // the finished match must not produce any further BoardUpdate.
    match.enqueue(PieceColor::White, ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});
    EXPECT_FALSE(white_sink.wait_for(200, is_board_update));

    match.stop();
}
