#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include "kfc/io/board_parser.hpp"
#include "kfc/model/board.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/json.hpp"
#include "kfc/server/room_manager.hpp"

using namespace kfc::model;
using namespace kfc::protocol;
using kfc::server::RoomManager;

namespace {

// One white pawn and one black pawn with room to move -- same fixture idea as
// test_match, enough for a real move to produce a BoardUpdate.
Board make_two_pawn_board() {
    return kfc::io::BoardParser().parse({
        ". . .",
        ". . .",
        "wP . bP",
    });
}

std::function<Board()> two_pawn_factory() {
    return [] { return make_two_pawn_board(); };
}

// Thread-safe recorder of everything a room sent to one connection -- the
// tick/broadcast happens on the room's own thread, assertions on the main one.
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

// The room id the server generated, read back out of the Welcome it sent -- the
// only way anyone (a test or a real client) learns a created room's id, now that
// the server mints it rather than the client naming it.
std::string room_id_from_welcome(const RecordingSink& sink) {
    for (const std::string& text : sink.snapshot()) {
        std::optional<ServerMessage> decoded = decode_server_message(text);
        if (decoded.has_value() && std::holds_alternative<Welcome>(*decoded)) {
            return std::get<Welcome>(*decoded).room;
        }
    }
    return {};
}

std::filesystem::path log_path() {
    return std::filesystem::temp_directory_path() / "kfc_room_manager_test.log";
}

}  // namespace

TEST(RoomManagerTest, TwoPlayersLandInTheSameRoomWithOpposedColors) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink alice;
    RecordingSink bob;
    std::optional<RoomManager::Seat> a = rooms.join_any("alice", 1200, alice.as_send_fn());
    std::optional<RoomManager::Seat> b = rooms.join_any("bob", 1200, bob.as_send_fn());

    ASSERT_TRUE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(a->room, b->room);
    EXPECT_EQ(a->color, PieceColor::White);
    EXPECT_EQ(b->color, PieceColor::Black);
    EXPECT_EQ(rooms.room_count(), 1u);
}

TEST(RoomManagerTest, AThirdPlayerOpensANewRoom) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3;
    std::optional<RoomManager::Seat> a = rooms.join_any("a", 1200, s1.as_send_fn());
    std::optional<RoomManager::Seat> b = rooms.join_any("b", 1200, s2.as_send_fn());
    std::optional<RoomManager::Seat> c = rooms.join_any("c", 1200, s3.as_send_fn());

    ASSERT_TRUE(a.has_value() && b.has_value() && c.has_value());
    EXPECT_EQ(a->room, b->room);
    EXPECT_NE(c->room, a->room);
    EXPECT_EQ(c->color, PieceColor::White);  // first seat of the fresh room
    EXPECT_EQ(rooms.room_count(), 2u);
}

TEST(RoomManagerTest, MovesAreRoutedOnlyToTheirOwnRoom) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    // Room 1: alice + bob. Room 2: carol + dave.
    RecordingSink alice, bob, carol, dave;
    std::optional<RoomManager::Seat> a = rooms.join_any("alice", 1200, alice.as_send_fn());
    std::optional<RoomManager::Seat> b = rooms.join_any("bob", 1200, bob.as_send_fn());
    std::optional<RoomManager::Seat> c = rooms.join_any("carol", 1200, carol.as_send_fn());
    std::optional<RoomManager::Seat> d = rooms.join_any("dave", 1200, dave.as_send_fn());
    ASSERT_TRUE(a && b && c && d);
    ASSERT_NE(a->room, c->room);

    // A legal white pawn move in room 1 only.
    rooms.enqueue(a->room, PieceColor::White, ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});

    EXPECT_TRUE(alice.wait_for(500, is_board_update));
    // Room 2 saw nothing -- the move never leaked across rooms.
    EXPECT_FALSE(carol.wait_for(200, is_board_update));
}

TEST(RoomManagerTest, RoomIsReapedOnceEveryPlayerHasDisconnected) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink alice, bob;
    std::optional<RoomManager::Seat> a = rooms.join_any("alice", 1200, alice.as_send_fn());
    std::optional<RoomManager::Seat> b = rooms.join_any("bob", 1200, bob.as_send_fn());
    ASSERT_TRUE(a && b);
    ASSERT_EQ(rooms.room_count(), 1u);

    // First disconnect ends the game but the other player is still in the room.
    rooms.on_disconnect(*a);
    EXPECT_EQ(rooms.room_count(), 1u);

    // Second disconnect empties the room -> it is torn down.
    rooms.on_disconnect(*b);
    EXPECT_EQ(rooms.room_count(), 0u);
}

TEST(RoomManagerTest, EnqueueToAReapedRoomIsASilentNoOp) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink alice, bob;
    std::optional<RoomManager::Seat> a = rooms.join_any("alice", 1200, alice.as_send_fn());
    std::optional<RoomManager::Seat> b = rooms.join_any("bob", 1200, bob.as_send_fn());
    ASSERT_TRUE(a && b);

    rooms.on_disconnect(*a);
    rooms.on_disconnect(*b);
    ASSERT_EQ(rooms.room_count(), 0u);

    // Routing a message to the now-gone room must not crash or resurrect it.
    rooms.enqueue(a->room, PieceColor::White, ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});
    EXPECT_EQ(rooms.room_count(), 0u);
}

TEST(RoomManagerTest, CreateAndJoinNamedRoomSeatWhiteThenBlackRegardlessOfRating) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink alice, bob;
    // Named rooms ignore rating entirely -- that's what Play is for.
    std::optional<RoomManager::Seat> a = rooms.create_room("alice", alice.as_send_fn());
    ASSERT_TRUE(a.has_value());
    // The creator's Welcome carries the id the server minted; that is what the
    // second player types in.
    std::string room_id = room_id_from_welcome(alice);
    ASSERT_FALSE(room_id.empty());
    std::optional<RoomManager::Seat> b = rooms.join_room(room_id, "bob", bob.as_send_fn());

    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(a->room, b->room);
    EXPECT_EQ(a->color, PieceColor::White);
    EXPECT_EQ(b->color, PieceColor::Black);
    EXPECT_EQ(rooms.room_count(), 1u);
}

TEST(RoomManagerTest, EveryCreatedRoomGetsItsOwnGeneratedId) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    // The client no longer names a room, so Create can never collide and never
    // fails -- each call simply mints another distinct id.
    RecordingSink s1, s2;
    ASSERT_TRUE(rooms.create_room("alice", s1.as_send_fn()).has_value());
    ASSERT_TRUE(rooms.create_room("bob", s2.as_send_fn()).has_value());

    std::string first = room_id_from_welcome(s1);
    std::string second = room_id_from_welcome(s2);
    EXPECT_FALSE(first.empty());
    EXPECT_FALSE(second.empty());
    EXPECT_NE(first, second);
    EXPECT_EQ(rooms.room_count(), 2u);
}

TEST(RoomManagerTest, AGeneratedRoomIdIsShortAndUnambiguousToReadOut) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    // One player has to read this id to another, so it stays short and avoids
    // the character pairs that get misheard or misread (0/O, 1/I/L, 5/S, ...).
    RecordingSink s1;
    ASSERT_TRUE(rooms.create_room("alice", s1.as_send_fn()).has_value());

    std::string id = room_id_from_welcome(s1);
    EXPECT_EQ(id.size(), 4u);
    for (char c : id) {
        // The pairs that actually get confused: 0/O, 1/I/L, 2/Z, 5/S, 8/B.
        EXPECT_EQ(std::string("01258BILOSZ").find(c), std::string::npos)
            << "id '" << id << "' contains an easily-confused character";
    }
}

TEST(RoomManagerTest, JoiningARoomThatDoesNotExistFailsWithAReason) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1;
    std::string reason;
    EXPECT_FALSE(rooms.join_room("ghost", "alice", s1.as_send_fn(), {}, &reason).has_value());
    EXPECT_EQ(reason, join_reasons::kNoSuchRoom);
}

TEST(RoomManagerTest, JoiningARoomWhoseGameIsAlreadyOverIsRefusedNotSeated) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3;
    std::optional<RoomManager::Seat> white = rooms.create_room("alice", s1.as_send_fn());
    ASSERT_TRUE(white.has_value());
    std::string room_id = room_id_from_welcome(s1);
    ASSERT_TRUE(rooms.join_room(room_id, "bob", s2.as_send_fn()).has_value());

    // Decide the game, and wait until the room has actually processed it.
    rooms.enqueue(white->room, PieceColor::White, ClientMessage{Resign{}});
    ASSERT_TRUE(s2.wait_for(2000, [](const ServerMessage& m) { return std::holds_alternative<GameOver>(m); }));

    // A latecomer must not be dropped into a board that will never move again --
    // neither as a player nor as a viewer.
    std::string reason;
    EXPECT_FALSE(rooms.join_room(room_id, "carol", s3.as_send_fn(), {}, &reason).has_value());
    EXPECT_EQ(reason, join_reasons::kRoomNotActive);
}

TEST(RoomManagerTest, JoinersAfterTheSecondBecomeSpectatorsOfTheSameRoom) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3, s4;
    std::optional<RoomManager::Seat> white = rooms.create_room("alice", s1.as_send_fn());
    ASSERT_TRUE(white.has_value());
    std::string room_id = room_id_from_welcome(s1);
    std::optional<RoomManager::Seat> black = rooms.join_room(room_id, "bob", s2.as_send_fn());
    // The spec: "the second person that joins the room is the Black player. The
    // following people who join are viewers."
    std::optional<RoomManager::Seat> carol = rooms.join_room(room_id, "carol", s3.as_send_fn());
    std::optional<RoomManager::Seat> dave = rooms.join_room(room_id, "dave", s4.as_send_fn());

    ASSERT_TRUE(black.has_value());
    EXPECT_FALSE(white->spectator);
    EXPECT_FALSE(black->spectator);

    ASSERT_TRUE(carol.has_value());
    ASSERT_TRUE(dave.has_value());
    EXPECT_TRUE(carol->spectator);
    EXPECT_TRUE(dave->spectator);
    // Viewers watch the game that is already there -- they don't spawn rooms.
    EXPECT_EQ(carol->room, white->room);
    EXPECT_EQ(dave->room, white->room);
    EXPECT_EQ(rooms.room_count(), 1u);
}

TEST(RoomManagerTest, ASpectatorIsToldItIsWatchingAndSeesTheGameStartedAlready) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3;
    ASSERT_TRUE(rooms.create_room("alice", s1.as_send_fn()).has_value());
    std::string room_id = room_id_from_welcome(s1);
    ASSERT_TRUE(rooms.join_room(room_id, "bob", s2.as_send_fn()).has_value());
    ASSERT_TRUE(rooms.join_room(room_id, "carol", s3.as_send_fn()).has_value());

    // Its Welcome says spectator, so the client knows not to accept input...
    EXPECT_TRUE(s3.wait_for(500, [](const ServerMessage& m) {
        return std::holds_alternative<Welcome>(m) && std::get<Welcome>(m).spectator;
    }));
    // ...and it is told the match is already under way, rather than being left
    // on a "searching for an opponent" screen for a game in full swing.
    EXPECT_TRUE(s3.wait_for(500, [](const ServerMessage& m) { return std::holds_alternative<MatchStart>(m); }));
}

TEST(RoomManagerTest, ASpectatorSeesTheSameBoardUpdatesThePlayersDo) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3;
    std::optional<RoomManager::Seat> white = rooms.create_room("alice", s1.as_send_fn());
    ASSERT_TRUE(white.has_value());
    std::string room_id = room_id_from_welcome(s1);
    ASSERT_TRUE(rooms.join_room(room_id, "bob", s2.as_send_fn()).has_value());
    std::optional<RoomManager::Seat> carol = rooms.join_room(room_id, "carol", s3.as_send_fn());
    ASSERT_TRUE(carol.has_value());

    // White pawn at (2,0) advances; the viewer must see the very same arrival.
    rooms.enqueue(white->room, PieceColor::White,
                  ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});

    EXPECT_TRUE(s1.wait_for(2000, is_board_update));
    EXPECT_TRUE(s3.wait_for(2000, is_board_update));
}

// --- Reconnect: telling a returning player apart from a stranger ---

// Builds a room whose Black player ("bob") has dropped and whose grace is
// counting down. Returns the room's generated id; the sinks are the caller's.
std::string room_with_black_mid_countdown(RoomManager& rooms, RecordingSink& white_sink,
                                           RecordingSink& black_sink) {
    std::optional<RoomManager::Seat> white = rooms.create_room("alice", white_sink.as_send_fn());
    EXPECT_TRUE(white.has_value());
    std::string room_id = room_id_from_welcome(white_sink);
    std::optional<RoomManager::Seat> black = rooms.join_room(room_id, "bob", black_sink.as_send_fn());
    EXPECT_TRUE(black.has_value());
    rooms.on_disconnect(*black);
    // Wait until the countdown is genuinely running, so the reclaim below is
    // testing the real state rather than racing it into existence.
    EXPECT_TRUE(white_sink.wait_for(2000, [](const ServerMessage& m) {
        return std::holds_alternative<OpponentDisconnected>(m);
    }));
    return room_id;
}

TEST(RoomManagerTest, TheDroppedPlayerReturningReclaimsTheirOwnSeatAndColor) {
    FileLogger logger(log_path());
    // A long grace, so the test is never racing the forfeit.
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink white_sink, black_sink, returning;
    std::string room_id = room_with_black_mid_countdown(rooms, white_sink, black_sink);

    // Same username -> this is bob coming back, not a third party.
    std::optional<RoomManager::Seat> back = rooms.join_room(room_id, "bob", returning.as_send_fn());
    ASSERT_TRUE(back.has_value());
    EXPECT_FALSE(back->spectator);
    EXPECT_EQ(back->color, PieceColor::Black);  // their own seat, not a new one

    // They get the game as it stands plus MatchStart, so they resume playing...
    EXPECT_TRUE(returning.wait_for(1000, [](const ServerMessage& m) {
        return std::holds_alternative<Welcome>(m) && !std::get<Welcome>(m).spectator &&
               std::get<Welcome>(m).assigned_color == PieceColor::Black;
    }));
    EXPECT_TRUE(returning.wait_for(1000, [](const ServerMessage& m) { return std::holds_alternative<MatchStart>(m); }));
    // ...and the opponent's countdown banner is cleared.
    EXPECT_TRUE(white_sink.wait_for(1000, [](const ServerMessage& m) {
        return std::holds_alternative<OpponentReconnected>(m);
    }));
}

TEST(RoomManagerTest, ReturningAfterTheGraceExpiredIsRefusedRatherThanSeated) {
    FileLogger logger(log_path());
    // A 100ms grace, so the expiry this test needs to observe costs 100ms
    // rather than the spec's twenty seconds.
    RoomManager rooms(two_pawn_factory(), logger, {}, {}, /*disconnect_grace_ms=*/100);

    RecordingSink white_sink, black_sink, too_late;
    std::optional<RoomManager::Seat> white = rooms.create_room("alice", white_sink.as_send_fn());
    ASSERT_TRUE(white.has_value());
    std::string room_id = room_id_from_welcome(white_sink);
    std::optional<RoomManager::Seat> black = rooms.join_room(room_id, "bob", black_sink.as_send_fn());
    ASSERT_TRUE(black.has_value());

    // Let the grace run all the way out, so the match is genuinely forfeit
    // before bob tries to come back.
    rooms.on_disconnect(*black);
    ASSERT_TRUE(white_sink.wait_for(3000, [](const ServerMessage& m) {
        return std::holds_alternative<GameOver>(m);
    }));

    // The seat is nobody's now. Reporting success here would leave bob's
    // connection believing it is seated in a match that already ended without
    // it -- and would leak the connection count, so the room could never reach
    // zero and never be reaped.
    std::string reason;
    EXPECT_FALSE(rooms.join_room(room_id, "bob", too_late.as_send_fn(), {}, &reason).has_value());
    EXPECT_EQ(reason, join_reasons::kRoomNotActive);
}

TEST(RoomManagerTest, AStrangerArrivingMidCountdownWatchesRatherThanTakingTheSeat) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink white_sink, black_sink, stranger;
    std::string room_id = room_with_black_mid_countdown(rooms, white_sink, black_sink);

    // A different username is just another joiner: the dropped player's seat is
    // theirs alone until their grace expires.
    std::optional<RoomManager::Seat> seat = rooms.join_room(room_id, "carol", stranger.as_send_fn());
    ASSERT_TRUE(seat.has_value());
    EXPECT_TRUE(seat->spectator);
    EXPECT_TRUE(stranger.wait_for(1000, [](const ServerMessage& m) {
        return std::holds_alternative<Welcome>(m) && std::get<Welcome>(m).spectator;
    }));
    // And the countdown carries on -- nothing about it was cancelled.
    EXPECT_FALSE(white_sink.wait_for(300, [](const ServerMessage& m) {
        return std::holds_alternative<OpponentReconnected>(m);
    }));
}

TEST(RoomManagerTest, ReturningInTimeCancelsTheForfeitAltogether) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink white_sink, black_sink, returning;
    std::string room_id = room_with_black_mid_countdown(rooms, white_sink, black_sink);

    ASSERT_TRUE(rooms.join_room(room_id, "bob", returning.as_send_fn()).has_value());

    // The default grace is 20s; well within it, no forfeit may ever arrive.
    EXPECT_FALSE(white_sink.wait_for(500, [](const ServerMessage& m) { return std::holds_alternative<GameOver>(m); }));
    EXPECT_EQ(rooms.room_count(), 1u);
}

TEST(RoomManagerTest, ASpectatorLeavingDoesNotForfeitThePlayersGame) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3;
    std::optional<RoomManager::Seat> white = rooms.create_room("alice", s1.as_send_fn());
    ASSERT_TRUE(white.has_value());
    std::string room_id = room_id_from_welcome(s1);
    ASSERT_TRUE(rooms.join_room(room_id, "bob", s2.as_send_fn()).has_value());
    std::optional<RoomManager::Seat> carol = rooms.join_room(room_id, "carol", s3.as_send_fn());
    ASSERT_TRUE(carol.has_value());

    // A viewer walking out holds no seat, so nothing about the game changes --
    // in particular White (whose colour the viewer's Seat nominally carries)
    // must not be handed a disconnect countdown, let alone a forfeit.
    rooms.on_disconnect(*carol);

    EXPECT_FALSE(s1.wait_for(300, [](const ServerMessage& m) {
        return std::holds_alternative<OpponentDisconnected>(m) || std::holds_alternative<GameOver>(m);
    }));
    EXPECT_EQ(rooms.room_count(), 1u);  // the room lives on for its two players
}

TEST(RoomManagerTest, ASpectatorThatLeftIsNoLongerSentTheGame) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3;
    ASSERT_TRUE(rooms.create_room("alice", s1.as_send_fn()).has_value());
    std::string room_id = room_id_from_welcome(s1);
    std::optional<RoomManager::Seat> black = rooms.join_room(room_id, "bob", s2.as_send_fn());
    ASSERT_TRUE(black.has_value());
    std::optional<RoomManager::Seat> carol = rooms.join_room(room_id, "carol", s3.as_send_fn());
    ASSERT_TRUE(carol.has_value());
    ASSERT_TRUE(carol->spectator);
    ASSERT_NE(carol->watcher, 0u) << "a viewer's Seat must carry the handle that identifies it";

    rooms.on_disconnect(*carol);
    std::size_t seen_before = s3.snapshot().size();

    // A real move, so the room genuinely broadcasts something afterwards.
    rooms.enqueue(black->room, PieceColor::White, ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});
    ASSERT_TRUE(s1.wait_for(2000, [](const ServerMessage& m) { return std::holds_alternative<BoardUpdate>(m); }));

    EXPECT_EQ(s3.snapshot().size(), seen_before)
        << "a viewer who disconnected must be dropped from the room's broadcasts, not merely ignored";
}

TEST(RoomManagerTest, PlayersMoreThanTheGapApartDoNotPairAndWaitSeparately) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2;
    // 1200 and 1400 are 200 apart -- beyond the ±100 gap -- so each waits alone.
    std::optional<RoomManager::Seat> a = rooms.join_any("a", 1200, s1.as_send_fn());
    std::optional<RoomManager::Seat> b = rooms.join_any("b", 1400, s2.as_send_fn());

    ASSERT_TRUE(a.has_value() && b.has_value());
    EXPECT_NE(a->room, b->room);
    EXPECT_EQ(a->color, PieceColor::White);
    EXPECT_EQ(b->color, PieceColor::White);  // each is alone in their own room
    EXPECT_EQ(rooms.room_count(), 2u);
}

TEST(RoomManagerTest, NewcomerPairsWithTheClosestRatedWaitingOpponent) {
    FileLogger logger(log_path());
    RoomManager rooms(two_pawn_factory(), logger);

    RecordingSink s1, s2, s3;
    // 1200 and 1360 are 160 apart -> both end up waiting in separate rooms.
    std::optional<RoomManager::Seat> a = rooms.join_any("a", 1200, s1.as_send_fn());
    std::optional<RoomManager::Seat> b = rooms.join_any("b", 1360, s2.as_send_fn());
    ASSERT_TRUE(a.has_value() && b.has_value());
    ASSERT_NE(a->room, b->room);

    // 1290 is within 100 of both (90 from a, 70 from b) -> pairs the closer, b.
    std::optional<RoomManager::Seat> c = rooms.join_any("c", 1290, s3.as_send_fn());
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(c->room, b->room);
    EXPECT_NE(c->room, a->room);
    EXPECT_EQ(c->color, PieceColor::Black);  // joined b's waiting room
    EXPECT_EQ(rooms.room_count(), 2u);       // a still waiting, b+c now playing
}
