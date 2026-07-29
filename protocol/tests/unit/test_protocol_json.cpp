#include <gtest/gtest.h>

#include "kfc/model/board.hpp"
#include "kfc/model/piece_names.hpp"
#include "kfc/protocol/json.hpp"
#include "kfc/realtime/motion_kind_names.hpp"

using namespace kfc::model;
using namespace kfc::protocol;

namespace {

Piece make_piece(int id, PieceColor color, PieceKind kind, Position cell) {
    return Piece{PieceId{id}, color, kind, cell, PieceState::Idle};
}

}  // namespace

TEST(ProtocolJsonTest, LoginRoundTripsUsernameAndPassword) {
    ClientMessage message = Login{"alice", "s3cret"};

    std::optional<ClientMessage> decoded = decode_client_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<Login>(*decoded));
    EXPECT_EQ(std::get<Login>(*decoded).username, "alice");
    EXPECT_EQ(std::get<Login>(*decoded).password, "s3cret");
}

TEST(ProtocolJsonTest, MoveRequestRoundTrips) {
    ClientMessage message = MoveRequest{Position{1, 2}, Position{3, 4}};

    std::optional<ClientMessage> decoded = decode_client_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<MoveRequest>(*decoded));
    const MoveRequest& request = std::get<MoveRequest>(*decoded);
    EXPECT_EQ(request.source, (Position{1, 2}));
    EXPECT_EQ(request.destination, (Position{3, 4}));
}

TEST(ProtocolJsonTest, JumpRequestRoundTrips) {
    ClientMessage message = JumpRequest{Position{5, 6}};

    std::optional<ClientMessage> decoded = decode_client_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<JumpRequest>(*decoded));
    EXPECT_EQ(std::get<JumpRequest>(*decoded).cell, (Position{5, 6}));
}

TEST(ProtocolJsonTest, MatchStartRoundTrips) {
    ServerMessage message = MatchStart{};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(std::holds_alternative<MatchStart>(*decoded));
}

TEST(ProtocolJsonTest, OpponentDisconnectedRoundTripsSecondsRemaining) {
    ServerMessage message = OpponentDisconnected{17};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<OpponentDisconnected>(*decoded));
    EXPECT_EQ(std::get<OpponentDisconnected>(*decoded).seconds_remaining, 17);
}

TEST(ProtocolJsonTest, ResignRoundTrips) {
    ClientMessage message = Resign{};

    std::optional<ClientMessage> decoded = decode_client_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(std::holds_alternative<Resign>(*decoded));
}

TEST(ProtocolJsonTest, PlayRoundTrips) {
    std::optional<ClientMessage> decoded = decode_client_message(encode(ClientMessage{Play{}}));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_TRUE(std::holds_alternative<Play>(*decoded));
}

TEST(ProtocolJsonTest, CreateCarriesNoNameAndJoinRoundTripsIt) {
    // Create names nothing: the server generates the id (see CreateRoom).
    std::optional<ClientMessage> created = decode_client_message(encode(ClientMessage{CreateRoom{}}));
    ASSERT_TRUE(created.has_value());
    EXPECT_TRUE(std::holds_alternative<CreateRoom>(*created));

    std::optional<ClientMessage> joined = decode_client_message(encode(ClientMessage{JoinRoom{"cockadoodle"}}));
    ASSERT_TRUE(joined.has_value());
    ASSERT_TRUE(std::holds_alternative<JoinRoom>(*joined));
    EXPECT_EQ(std::get<JoinRoom>(*joined).name, "cockadoodle");
}

TEST(ProtocolJsonTest, WelcomeRoundTripsAssignedColorAndBoard) {
    Board board(3, 2);
    board.add_piece(make_piece(1, PieceColor::White, PieceKind::King, Position{0, 0}));
    board.add_piece(make_piece(2, PieceColor::Black, PieceKind::Pawn, Position{1, 2}));
    ServerMessage message = Welcome{PieceColor::Black, snapshot_of(board)};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<Welcome>(*decoded));
    const Welcome& welcome = std::get<Welcome>(*decoded);
    EXPECT_EQ(welcome.assigned_color, PieceColor::Black);
    EXPECT_EQ(welcome.board.width, 3);
    EXPECT_EQ(welcome.board.height, 2);
    ASSERT_EQ(welcome.board.pieces.size(), 2u);
}

TEST(ProtocolJsonTest, SnapshotOfCapturesEveryOccupiedCellExactly) {
    Board board(2, 2);
    board.add_piece(make_piece(7, PieceColor::White, PieceKind::Queen, Position{0, 1}));

    BoardSnapshot snapshot = snapshot_of(board);

    ASSERT_EQ(snapshot.pieces.size(), 1u);
    EXPECT_EQ(snapshot.pieces[0].id, PieceId{7});
    EXPECT_EQ(snapshot.pieces[0].kind, PieceKind::Queen);
    EXPECT_EQ(snapshot.pieces[0].cell, (Position{0, 1}));
}

TEST(ProtocolJsonTest, BoardUpdateRoundTripsArrivalEventsIncludingACapture) {
    ArrivalEvent event;
    event.moved_piece = make_piece(1, PieceColor::White, PieceKind::Rook, Position{2, 2});
    event.source = Position{2, 0};
    event.destination = Position{2, 2};
    event.captured_piece = make_piece(2, PieceColor::Black, PieceKind::Pawn, Position{2, 2});
    event.captured_piece->state = PieceState::Captured;
    event.arrived_at_ms = 1500;
    ServerMessage message = BoardUpdate{{event}};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<BoardUpdate>(*decoded));
    const BoardUpdate& update = std::get<BoardUpdate>(*decoded);
    ASSERT_EQ(update.arrival_events.size(), 1u);
    const ArrivalEvent& round_tripped = update.arrival_events[0];
    EXPECT_EQ(round_tripped.moved_piece.id, PieceId{1});
    EXPECT_EQ(round_tripped.source, (Position{2, 0}));
    EXPECT_EQ(round_tripped.destination, (Position{2, 2}));
    EXPECT_EQ(round_tripped.arrived_at_ms, 1500);
    ASSERT_TRUE(round_tripped.captured_piece.has_value());
    EXPECT_EQ(round_tripped.captured_piece->id, PieceId{2});
    EXPECT_EQ(round_tripped.captured_piece->state, PieceState::Captured);
    EXPECT_EQ(round_tripped.kind, MotionKind::Move);
    EXPECT_FALSE(round_tripped.was_promotion);
}

TEST(ProtocolJsonTest, BoardUpdateRoundTripsAJumpAndAPromotionFlag) {
    ArrivalEvent jump;
    jump.moved_piece = make_piece(1, PieceColor::White, PieceKind::Knight, Position{4, 4});
    jump.source = Position{4, 4};
    jump.destination = Position{4, 4};
    jump.kind = MotionKind::JumpInPlace;

    ArrivalEvent promo;
    promo.moved_piece = make_piece(2, PieceColor::White, PieceKind::Queen, Position{0, 4});
    promo.source = Position{1, 4};
    promo.destination = Position{0, 4};
    promo.was_promotion = true;

    ServerMessage message = BoardUpdate{{jump, promo}};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    const BoardUpdate& update = std::get<BoardUpdate>(*decoded);
    ASSERT_EQ(update.arrival_events.size(), 2u);
    EXPECT_EQ(update.arrival_events[0].kind, MotionKind::JumpInPlace);
    EXPECT_FALSE(update.arrival_events[0].was_promotion);
    EXPECT_EQ(update.arrival_events[1].kind, MotionKind::Move);
    EXPECT_TRUE(update.arrival_events[1].was_promotion);
}

TEST(ProtocolJsonTest, BoardUpdateRoundTripsAnArrivalWithNoCapture) {
    ArrivalEvent event;
    event.moved_piece = make_piece(1, PieceColor::White, PieceKind::Rook, Position{0, 0});
    event.source = Position{0, 0};
    event.destination = Position{0, 0};
    event.arrived_at_ms = 0;
    ServerMessage message = BoardUpdate{{event}};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    const BoardUpdate& update = std::get<BoardUpdate>(*decoded);
    ASSERT_EQ(update.arrival_events.size(), 1u);
    EXPECT_FALSE(update.arrival_events[0].captured_piece.has_value());
}

TEST(ProtocolJsonTest, MotionStartedRoundTripsAMoveMotion) {
    Motion motion;
    motion.moving_piece = make_piece(3, PieceColor::White, PieceKind::Rook, Position{4, 4});
    motion.source = Position{4, 4};
    motion.destination = Position{4, 6};
    motion.kind = MotionKind::Move;
    motion.duration_ms = 2000;
    motion.elapsed_ms = 0;
    motion.cooldown_ms = 500;
    ServerMessage message = MotionStarted{motion};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<MotionStarted>(*decoded));
    const Motion& round_tripped = std::get<MotionStarted>(*decoded).motion;
    EXPECT_EQ(round_tripped.moving_piece.id, PieceId{3});
    EXPECT_EQ(round_tripped.source, (Position{4, 4}));
    EXPECT_EQ(round_tripped.destination, (Position{4, 6}));
    EXPECT_EQ(round_tripped.kind, MotionKind::Move);
    EXPECT_EQ(round_tripped.duration_ms, 2000);
    EXPECT_EQ(round_tripped.elapsed_ms, 0);
    EXPECT_EQ(round_tripped.cooldown_ms, 500);
}

TEST(ProtocolJsonTest, MotionStartedRoundTripsAJumpInPlaceMotion) {
    Motion motion;
    motion.moving_piece = make_piece(9, PieceColor::Black, PieceKind::Pawn, Position{1, 1});
    motion.source = Position{1, 1};
    motion.destination = Position{1, 1};
    motion.kind = MotionKind::JumpInPlace;
    motion.duration_ms = 700;
    motion.elapsed_ms = 0;
    motion.cooldown_ms = 250;
    ServerMessage message = MotionStarted{motion};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    const Motion& round_tripped = std::get<MotionStarted>(*decoded).motion;
    EXPECT_EQ(round_tripped.kind, MotionKind::JumpInPlace);
    EXPECT_EQ(round_tripped.source, round_tripped.destination);
}

TEST(ProtocolJsonTest, MoveRejectedRoundTripsTheReasonVerbatim) {
    ServerMessage message = MoveRejected{"motion_in_progress"};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<MoveRejected>(*decoded));
    EXPECT_EQ(std::get<MoveRejected>(*decoded).reason, "motion_in_progress");
}

TEST(ProtocolJsonTest, GameOverRoundTripsAWinner) {
    ServerMessage message = GameOver{PieceColor::White};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    ASSERT_TRUE(std::holds_alternative<GameOver>(*decoded));
    ASSERT_TRUE(std::get<GameOver>(*decoded).winner.has_value());
    EXPECT_EQ(*std::get<GameOver>(*decoded).winner, PieceColor::White);
}

TEST(ProtocolJsonTest, GameOverRoundTripsADrawAsNoWinner) {
    ServerMessage message = GameOver{std::nullopt};

    std::optional<ServerMessage> decoded = decode_server_message(encode(message));

    ASSERT_TRUE(decoded.has_value());
    EXPECT_FALSE(std::get<GameOver>(*decoded).winner.has_value());
}

TEST(ProtocolJsonTest, DecodeClientMessageReturnsNulloptForGarbageText) {
    EXPECT_FALSE(decode_client_message("not json at all").has_value());
    EXPECT_FALSE(decode_client_message(R"({"type": "Login"})").has_value());  // missing payload
    EXPECT_FALSE(decode_client_message(R"({"type": "NotARealType", "payload": {}})").has_value());
}

TEST(ProtocolJsonTest, DecodeServerMessageReturnsNulloptForGarbageText) {
    EXPECT_FALSE(decode_server_message("").has_value());
    EXPECT_FALSE(decode_server_message(R"({"type": "GameOver"})").has_value());  // missing payload
}

// --- Every enumerator is writable and readable, not just the ones in use ---

// Driven off the name tables themselves, so an enumerator added tomorrow is
// covered the moment it is given a name -- rather than needing someone to
// remember to extend this test too. The tables' own static_asserts guarantee
// no enumerator is missing from them; this checks the codec agrees.
TEST(ProtocolJsonTest, EveryPieceEnumeratorSurvivesTheWireInBothDirections) {
    Board board(8, 8);
    int id = 0;
    int cell = 0;
    for (const auto& [kind, kind_name] : kPieceKindNames.entries) {
        for (const auto& [color, color_name] : kPieceColorNames.entries) {
            for (const auto& [state, state_name] : kPieceStateNames.entries) {
                Piece piece = make_piece(++id, color, kind, Position{cell / 8, cell % 8});
                piece.state = state;
                board.add_piece(piece);
                ++cell;
            }
        }
    }
    BoardSnapshot sent = snapshot_of(board);
    ASSERT_EQ(sent.pieces.size(), 7u * 2u * 4u) << "precondition: one piece per combination";

    std::optional<ServerMessage> decoded =
        decode_server_message(encode(ServerMessage{Welcome{PieceColor::White, sent}}));

    ASSERT_TRUE(decoded.has_value());
    const std::vector<Piece>& received = std::get<Welcome>(*decoded).board.pieces;
    ASSERT_EQ(received.size(), sent.pieces.size());
    for (std::size_t i = 0; i < sent.pieces.size(); ++i) {
        EXPECT_EQ(received[i].kind, sent.pieces[i].kind);
        EXPECT_EQ(received[i].color, sent.pieces[i].color);
        EXPECT_EQ(received[i].state, sent.pieces[i].state);
    }
}

TEST(ProtocolJsonTest, EveryMotionKindSurvivesTheWireInBothDirections) {
    std::vector<ArrivalEvent> events;
    for (const auto& [kind, name] : kMotionKindNames.entries) {
        ArrivalEvent event;
        event.moved_piece = make_piece(1, PieceColor::White, PieceKind::Knight, Position{3, 3});
        event.source = Position{3, 3};
        event.destination = Position{3, 3};
        event.kind = kind;
        events.push_back(event);
    }

    std::optional<ServerMessage> decoded = decode_server_message(encode(ServerMessage{BoardUpdate{events}}));

    ASSERT_TRUE(decoded.has_value());
    const BoardUpdate& update = std::get<BoardUpdate>(*decoded);
    ASSERT_EQ(update.arrival_events.size(), kMotionKindNames.entries.size());
    for (std::size_t i = 0; i < events.size(); ++i) {
        EXPECT_EQ(update.arrival_events[i].kind, events[i].kind);
    }
}

// A name no table knows must fail the whole message rather than quietly
// becoming whatever the first enumerator happens to be -- an untrusted client
// could otherwise turn a typo into a piece the board never had.
TEST(ProtocolJsonTest, AnUnknownEnumNameIsRefusedRatherThanDefaulted) {
    auto welcome_with = [](const char* kind) {
        return std::string(R"({"type":"Welcome","payload":{"assigned_color":"White","board":{"width":1,"height":1,)"
                           R"("pieces":[{"id":1,"color":"White","kind":")") +
               kind + R"(","cell":{"row":0,"col":0},"state":"Idle","has_moved":false}]}}})";
    };

    ASSERT_TRUE(decode_server_message(welcome_with("Rook")).has_value()) << "precondition: the shape is valid";
    EXPECT_FALSE(decode_server_message(welcome_with("Wizard")).has_value());
    EXPECT_FALSE(decode_server_message(welcome_with("rook")).has_value()) << "names are case-sensitive";
    EXPECT_FALSE(decode_server_message(welcome_with("")).has_value());
}

// --- What a peer is allowed to send at all ---

// Parsing is where an untrusted peer gets to make this process allocate, so
// the length is checked before the parser ever sees the text. A gigabyte of
// valid JSON is still a gigabyte.
TEST(ProtocolJsonTest, AMessageLongerThanTheLimitIsRefusedWithoutParsing) {
    std::string padding(kMaxMessageBytes, 'x');
    std::string oversized = R"({"type":"MoveRejected","payload":{"reason":")" + padding + R"("}})";
    ASSERT_GT(oversized.size(), kMaxMessageBytes);

    EXPECT_FALSE(decode_server_message(oversized).has_value());
    EXPECT_FALSE(decode_client_message(oversized).has_value());
}

TEST(ProtocolJsonTest, AMessageAtTheLimitIsStillAccepted) {
    // Padded to exactly the limit, so the boundary itself is exercised rather
    // than assumed -- an off-by-one here silently refuses legitimate traffic.
    // The padding is measured from the real message, not counted by hand.
    auto message_with = [](const std::string& reason) {
        return R"({"type":"MoveRejected","payload":{"reason":")" + reason + R"("}})";
    };
    std::string reason(kMaxMessageBytes - message_with("").size(), 'x');
    std::string message = message_with(reason);
    ASSERT_EQ(message.size(), kMaxMessageBytes);

    std::optional<ServerMessage> decoded = decode_server_message(message);
    ASSERT_TRUE(decoded.has_value());
    EXPECT_EQ(std::get<MoveRejected>(*decoded).reason.size(), reason.size());
}

// --- Never write a password into a log file ---

TEST(RedactForLogTest, StripsThePasswordFromAnEncodedLogin) {
    std::string encoded = encode(ClientMessage{Login{"alice", "hunter2"}});
    ASSERT_NE(encoded.find("hunter2"), std::string::npos) << "precondition: the wire form does carry it";

    std::string safe = redact_for_log(encoded);
    EXPECT_EQ(safe.find("hunter2"), std::string::npos);
    // The username is still there -- redaction must not cost us the log's value.
    EXPECT_NE(safe.find("alice"), std::string::npos);
    EXPECT_NE(safe.find("***"), std::string::npos);
}

TEST(RedactForLogTest, LeavesMessagesWithoutAPasswordUntouched) {
    std::string encoded = encode(ClientMessage{MoveRequest{Position{2, 0}, Position{1, 0}}});
    EXPECT_EQ(redact_for_log(encoded), encoded);
}

TEST(RedactForLogTest, StripsAPasswordContainingAnEscapedQuote) {
    // The escape must be stepped over, not treated as the closing quote --
    // otherwise the tail of the password lands in the log.
    std::string encoded = encode(ClientMessage{Login{"alice", "a\"b\"c"}});
    std::string safe = redact_for_log(encoded);
    EXPECT_EQ(safe.find("b"), std::string::npos) << "leaked part of the password: " << safe;
    EXPECT_NE(safe.find("alice"), std::string::npos);
}

TEST(RedactForLogTest, RedactsEvenWhenTheMessageCannotBeDecoded) {
    // A malformed or newer-than-us Login must still come out safe: the whole
    // point is that a decode failure cannot become a leak.
    std::string malformed = R"({"type":"Login","payload":{"password":"s3cret","username":"bob",)";
    ASSERT_FALSE(decode_client_message(malformed).has_value());
    EXPECT_EQ(redact_for_log(malformed).find("s3cret"), std::string::npos);
}

// Found by pointing a hand-written client at the running server: its JSON had a
// space after the colon, the scan was matching the exact bytes our own encoder
// emits, and the password went into kfc_server.log in clear. Redaction exists
// precisely for messages we did not produce, so it must not assume our format.
TEST(RedactForLogTest, StripsThePasswordHoweverThePeerSpacedItsJson) {
    for (const std::string& spaced : {
             R"({"type":"Login","payload":{"username":"alice","password": "hunter2"}})",
             R"({"type":"Login","payload":{"username":"alice","password" : "hunter2"}})",
             R"({"type":"Login","payload":{"username":"alice","password":    "hunter2"}})",
             "{\"type\":\"Login\",\"payload\":{\"password\":\n\t\"hunter2\"}}",
         }) {
        std::string safe = redact_for_log(spaced);
        EXPECT_EQ(safe.find("hunter2"), std::string::npos) << "leaked from: " << spaced;
        EXPECT_NE(safe.find("***"), std::string::npos) << "nothing was redacted in: " << spaced;
    }
}

// A password that is not a quoted string is still a secret.
TEST(RedactForLogTest, StripsAPasswordThatIsNotAString) {
    std::string numeric = R"({"type":"Login","payload":{"password":12345,"username":"alice"}})";
    std::string safe = redact_for_log(numeric);
    EXPECT_EQ(safe.find("12345"), std::string::npos);
    EXPECT_NE(safe.find("alice"), std::string::npos) << "redaction must not cost the log its value";
}

// The word appearing as a value, not a key, names no secret -- redacting it
// would quietly corrupt an unrelated message.
TEST(RedactForLogTest, LeavesTheWordPasswordAloneWhenItIsNotAKey) {
    std::string message = R"({"type":"MoveRejected","payload":{"reason":"password"}})";
    EXPECT_EQ(redact_for_log(message), message);
}

// The exact line that appeared in kfc_server.log, kept verbatim so the specific
// traffic that leaked can never leak again.
TEST(RedactForLogTest, StripsThePasswordFromTheLineThatActuallyLeaked) {
    std::string leaked = R"({"v": 1, "type": "Login", "payload": {"username": "probe2", "password": "pw"}})";

    std::string safe = redact_for_log(leaked);

    EXPECT_EQ(safe.find("\"pw\""), std::string::npos) << "still leaking: " << safe;
    EXPECT_NE(safe.find("probe2"), std::string::npos) << "the username is what makes the line worth logging";
}
