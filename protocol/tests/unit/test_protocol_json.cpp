#include <gtest/gtest.h>

#include "kfc/model/board.hpp"
#include "kfc/protocol/json.hpp"

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
