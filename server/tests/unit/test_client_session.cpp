#include <gtest/gtest.h>

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

#include "kfc/database/user_repository.hpp"
#include "kfc/io/board_parser.hpp"
#include "kfc/model/board.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/json.hpp"
#include "kfc/server/client_session.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/server/session_registry.hpp"

using namespace kfc::model;
using namespace kfc::protocol;
using kfc::server::ClientSession;
using kfc::server::RoomManager;

namespace {

std::filesystem::path fresh_path(const std::string& stem) {
    static int counter = 0;
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("kfc_session_" + stem + std::to_string(counter++));
    std::filesystem::remove(path);
    return path;
}

std::function<Board()> two_pawn_factory() {
    return [] {
        return kfc::io::BoardParser().parse({
            ". . .",
            ". . .",
            "wP . bP",
        });
    };
}

// Stands in for the socket. A ClientSession reaches its client only through
// these two callbacks, which is the whole reason it can be tested without
// IXWebSocket anywhere in sight.
class FakeSocket {
public:
    kfc::server::SendFn send_fn() {
        return [this](const std::string& text) { sent.push_back(text); };
    }

    kfc::server::CloseFn close_fn() {
        return [this] { ++closes; };
    }

    // The first message of this type the client was sent, if any.
    template <typename T>
    [[nodiscard]] std::optional<T> first_of() const {
        for (const std::string& text : sent) {
            std::optional<ServerMessage> decoded = decode_server_message(text);
            if (decoded.has_value() && std::holds_alternative<T>(*decoded)) {
                return std::get<T>(*decoded);
            }
        }
        return std::nullopt;
    }

    std::vector<std::string> sent;
    int closes = 0;
};

// Everything a session needs behind it, built fresh per test.
struct Fixture {
    Fixture()
        : logger(fresh_path("log")),
          users(fresh_path("db").string()),
          rooms(two_pawn_factory(), logger) {}

    ClientSession session(FakeSocket& socket, const std::string& id = "conn-1") {
        return ClientSession(id, socket.send_fn(), socket.close_fn(), rooms, users, sessions, logger);
    }

    FileLogger logger;
    kfc::database::UserRepository users;
    RoomManager rooms;
    kfc::server::SessionRegistry sessions;
};

std::string login_text(const std::string& user, const std::string& password) {
    return encode(ClientMessage{Login{user, password}});
}

}  // namespace

// --- Nothing happens before a Login ---

TEST(ClientSessionTest, ASeatingRequestBeforeLoginIsIgnored) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(encode(ClientMessage{Play{}}));

    EXPECT_FALSE(session.seat().has_value());
    EXPECT_TRUE(socket.sent.empty()) << "an unauthenticated connection must not be seated";
    EXPECT_EQ(socket.closes, 0) << "ignored, not hung up on -- the client may still log in";
    EXPECT_EQ(fixture.rooms.room_count(), 0u);
}

TEST(ClientSessionTest, AMoveBeforeBeingSeatedIsIgnored) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_text(encode(ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}}));

    EXPECT_TRUE(session.authenticated());
    EXPECT_FALSE(session.seat().has_value());
    EXPECT_TRUE(socket.sent.empty());
}

// --- Login ---

TEST(ClientSessionTest, AFirstLoginRegistersTheAccountAndSaysNothingBack) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));

    EXPECT_TRUE(session.authenticated());
    EXPECT_TRUE(socket.sent.empty()) << "success is silent; the client waits for a Welcome after it asks to be seated";
    EXPECT_EQ(socket.closes, 0);
}

// A bare hang-up would leave the client to wait out its own timeout and then
// blame whatever it tried next. The reason has to reach it first.
TEST(ClientSessionTest, AWrongPasswordIsExplainedBeforeTheConnectionIsClosed) {
    Fixture fixture;
    FakeSocket first_socket;
    ClientSession registering = fixture.session(first_socket);
    registering.on_text(login_text("alice", "correct"));

    FakeSocket socket;
    ClientSession session = fixture.session(socket, "conn-2");
    session.on_text(login_text("alice", "wrong"));

    std::optional<LoginFailed> failure = socket.first_of<LoginFailed>();
    ASSERT_TRUE(failure.has_value()) << "the client was hung up on without being told why";
    EXPECT_EQ(failure->reason, "wrong_password");
    EXPECT_EQ(socket.closes, 1);
    EXPECT_FALSE(session.authenticated());
}

TEST(ClientSessionTest, ASecondLoginOnTheSameConnectionChangesNothing) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_text(login_text("mallory", "pw"));  // cannot become someone else

    session.on_text(encode(ClientMessage{CreateRoom{}}));
    std::optional<Welcome> welcome = socket.first_of<Welcome>();
    ASSERT_TRUE(welcome.has_value());
    EXPECT_EQ(socket.closes, 0);
}

// --- Seating ---

TEST(ClientSessionTest, CreateSeatsTheClientAsWhiteAndSendsAWelcome) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_text(encode(ClientMessage{CreateRoom{}}));

    ASSERT_TRUE(session.seat().has_value());
    EXPECT_EQ(session.seat()->color, PieceColor::White);
    EXPECT_FALSE(session.seat()->spectator);

    std::optional<Welcome> welcome = socket.first_of<Welcome>();
    ASSERT_TRUE(welcome.has_value());
    EXPECT_FALSE(welcome->room.empty()) << "the creator has to be told the id to read out";
}

TEST(ClientSessionTest, JoiningARoomThatDoesNotExistIsExplainedThenClosed) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_text(encode(ClientMessage{JoinRoom{"NOPE"}}));

    std::optional<JoinFailed> failure = socket.first_of<JoinFailed>();
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->reason, join_reasons::kNoSuchRoom);
    EXPECT_EQ(socket.closes, 1);
    EXPECT_FALSE(session.seat().has_value());
}

// An empty name is refused without troubling RoomManager -- there is no room it
// could mean, and the client still gets a reason rather than a silent close.
TEST(ClientSessionTest, JoiningWithAnEmptyRoomNameIsRefusedWithAReason) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_text(encode(ClientMessage{JoinRoom{""}}));

    std::optional<JoinFailed> failure = socket.first_of<JoinFailed>();
    ASSERT_TRUE(failure.has_value());
    EXPECT_EQ(failure->reason, join_reasons::kNoSuchRoom);
    EXPECT_EQ(fixture.rooms.room_count(), 0u) << "an empty name must not open or touch a room";
}

TEST(ClientSessionTest, ASecondSeatingRequestIsIgnoredRatherThanTakingAnotherSeat) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_text(encode(ClientMessage{CreateRoom{}}));
    ASSERT_TRUE(session.seat().has_value());
    kfc::server::RoomId first_room = session.seat()->room;

    session.on_text(encode(ClientMessage{CreateRoom{}}));

    EXPECT_EQ(session.seat()->room, first_room);
    EXPECT_EQ(fixture.rooms.room_count(), 1u) << "one connection must not be able to open rooms in a loop";
}

// --- What a viewer may not do ---

TEST(ClientSessionTest, AViewersGameplayMessagesAreDroppedRatherThanRouted) {
    Fixture fixture;
    FakeSocket white_socket, black_socket, viewer_socket;

    ClientSession white = fixture.session(white_socket, "conn-white");
    white.on_text(login_text("alice", "pw"));
    white.on_text(encode(ClientMessage{CreateRoom{}}));
    ASSERT_TRUE(white.seat().has_value());
    std::optional<Welcome> welcome = white_socket.first_of<Welcome>();
    ASSERT_TRUE(welcome.has_value());
    const std::string room = welcome->room;

    ClientSession black = fixture.session(black_socket, "conn-black");
    black.on_text(login_text("bob", "pw"));
    black.on_text(encode(ClientMessage{JoinRoom{room}}));
    ASSERT_TRUE(black.seat().has_value());

    ClientSession viewer = fixture.session(viewer_socket, "conn-viewer");
    viewer.on_text(login_text("carol", "pw"));
    viewer.on_text(encode(ClientMessage{JoinRoom{room}}));
    ASSERT_TRUE(viewer.seat().has_value());
    ASSERT_TRUE(viewer.seat()->spectator) << "precondition: the third joiner watches";

    // A viewer's Seat nominally carries White's colour. Routing its Resign
    // would end someone else's game for them.
    viewer.on_text(encode(ClientMessage{Resign{}}));

    EXPECT_FALSE(white_socket.first_of<GameOver>().has_value());
    EXPECT_FALSE(black_socket.first_of<GameOver>().has_value());
}

// --- Frames we refuse to handle at all ---

TEST(ClientSessionTest, AnOversizedFrameClosesTheConnectionWithoutBeingParsed) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(std::string(kMaxMessageBytes + 1, 'x'));

    EXPECT_EQ(socket.closes, 1) << "the sender must be hung up on, not merely ignored";
    EXPECT_FALSE(session.authenticated());
}

TEST(ClientSessionTest, AnUndecodableFrameIsDroppedButTheConnectionSurvives) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text("not json at all");
    session.on_text(R"({"type":"NotARealType","payload":{}})");

    EXPECT_EQ(socket.closes, 0) << "a garbled frame is not grounds for hanging up";
    // ...and the connection still works afterwards.
    session.on_text(login_text("alice", "pw"));
    EXPECT_TRUE(session.authenticated());
}

// --- Closing ---

TEST(ClientSessionTest, ClosingBeforeBeingSeatedEndsNothing) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_close();

    EXPECT_EQ(fixture.rooms.room_count(), 0u);
}

TEST(ClientSessionTest, ClosingAfterBeingSeatedReleasesTheRoom) {
    Fixture fixture;
    FakeSocket socket;
    ClientSession session = fixture.session(socket);

    session.on_text(login_text("alice", "pw"));
    session.on_text(encode(ClientMessage{CreateRoom{}}));
    ASSERT_EQ(fixture.rooms.room_count(), 1u);

    session.on_close();

    EXPECT_EQ(fixture.rooms.room_count(), 0u) << "the room outlived its only occupant";
}

// --- One account, one live session ---

TEST(ClientSessionTest, ASecondConnectionForALoggedInAccountIsRefused) {
    Fixture fixture;
    FakeSocket first_socket;
    ClientSession first = fixture.session(first_socket, "conn-1");
    first.on_text(login_text("alice", "pw"));
    ASSERT_TRUE(first.authenticated());

    FakeSocket second_socket;
    ClientSession second = fixture.session(second_socket, "conn-2");
    second.on_text(login_text("alice", "pw"));  // right password, wrong moment

    std::optional<LoginFailed> failure = second_socket.first_of<LoginFailed>();
    ASSERT_TRUE(failure.has_value()) << "the duplicate was hung up on without being told why";
    EXPECT_EQ(failure->reason, login_reasons::kAlreadyLoggedIn);
    EXPECT_EQ(second_socket.closes, 1);
    EXPECT_FALSE(second.authenticated());
    EXPECT_EQ(fixture.sessions.live_count(), 1u);
}

TEST(ClientSessionTest, TheDuplicateIsRefusedRatherThanTheOriginalBeingKicked) {
    Fixture fixture;
    FakeSocket first_socket;
    ClientSession first = fixture.session(first_socket, "conn-1");
    first.on_text(login_text("alice", "pw"));
    first.on_text(encode(ClientMessage{CreateRoom{}}));
    ASSERT_TRUE(first.seat().has_value());

    FakeSocket second_socket;
    ClientSession second = fixture.session(second_socket, "conn-2");
    second.on_text(login_text("alice", "pw"));

    // Kicking the first is the other plausible policy, and it is the wrong one:
    // its close arrives asynchronously and would report a disconnect against a
    // seat the newcomer had meanwhile reclaimed. The player already playing must
    // not be disturbed by someone else logging in as them.
    EXPECT_EQ(first_socket.closes, 0) << "the rightful session was kicked";
    EXPECT_TRUE(first.seat().has_value());
    EXPECT_EQ(fixture.rooms.room_count(), 1u);
}

// The invariant that keeps this from fighting with reconnection: the name is
// held for exactly as long as the connection is, and comes back the instant it
// closes -- which is the same instant the disconnect grace starts. A player
// mid-countdown can therefore always log in again to reclaim their seat.
TEST(ClientSessionTest, ClosingAConnectionFreesTheNameForTheSameAccountToReturn) {
    Fixture fixture;
    std::string room;
    {
        FakeSocket socket;
        ClientSession dropped = fixture.session(socket, "conn-1");
        dropped.on_text(login_text("alice", "pw"));
        dropped.on_text(encode(ClientMessage{CreateRoom{}}));
        std::optional<Welcome> welcome = socket.first_of<Welcome>();
        ASSERT_TRUE(welcome.has_value());
        room = welcome->room;

        dropped.on_close();
        EXPECT_EQ(fixture.sessions.live_count(), 0u) << "the name was still held after the socket closed";
    }

    FakeSocket returning_socket;
    ClientSession returning = fixture.session(returning_socket, "conn-2");
    returning.on_text(login_text("alice", "pw"));

    EXPECT_TRUE(returning.authenticated()) << "a returning player was locked out by their own old session";
    EXPECT_TRUE(returning_socket.first_of<LoginFailed>() == std::nullopt);
}

TEST(ClientSessionTest, AWrongPasswordDoesNotClaimTheName) {
    Fixture fixture;
    FakeSocket registering_socket;
    ClientSession registering = fixture.session(registering_socket, "conn-0");
    registering.on_text(login_text("alice", "correct"));
    registering.on_close();
    ASSERT_EQ(fixture.sessions.live_count(), 0u);

    FakeSocket guesser_socket;
    ClientSession guesser = fixture.session(guesser_socket, "conn-1");
    guesser.on_text(login_text("alice", "wrong"));

    // Otherwise anyone could lock a real account out simply by guessing at its
    // name and getting the password wrong on purpose.
    EXPECT_EQ(fixture.sessions.live_count(), 0u) << "a failed login reserved the name anyway";

    FakeSocket owner_socket;
    ClientSession owner = fixture.session(owner_socket, "conn-2");
    owner.on_text(login_text("alice", "correct"));
    EXPECT_TRUE(owner.authenticated());
}

TEST(ClientSessionTest, DestroyingASessionWithoutACloseStillFreesTheName) {
    Fixture fixture;
    {
        FakeSocket socket;
        ClientSession abandoned = fixture.session(socket);
        abandoned.on_text(login_text("alice", "pw"));
        ASSERT_EQ(fixture.sessions.live_count(), 1u);
    }
    // The lease is RAII precisely so a path that forgets to close cannot lock an
    // account out permanently.
    EXPECT_EQ(fixture.sessions.live_count(), 0u);
}

TEST(ClientSessionTest, DifferentAccountsAreUnaffectedByEachOther) {
    Fixture fixture;
    FakeSocket alice_socket, bob_socket;
    ClientSession alice = fixture.session(alice_socket, "conn-1");
    ClientSession bob = fixture.session(bob_socket, "conn-2");

    alice.on_text(login_text("alice", "pw"));
    bob.on_text(login_text("bob", "pw"));

    EXPECT_TRUE(alice.authenticated());
    EXPECT_TRUE(bob.authenticated());
    EXPECT_EQ(fixture.sessions.live_count(), 2u);
}
