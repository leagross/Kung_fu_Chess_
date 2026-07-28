#include <gtest/gtest.h>

#include "kfc/realtime/game_over_observer.hpp"
#include "kfc/realtime/move_log_observer.hpp"
#include "kfc/realtime/score_observer.hpp"

using namespace kfc::model;

namespace {
Piece make_piece(PieceKind kind, int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, kind, cell, PieceState::Idle};
}

ArrivalEvent make_event(const Piece& moved, Position source, Position destination,
                         std::optional<Piece> captured = std::nullopt, long long arrived_at_ms = 0) {
    // Designated init so this stays correct regardless of where the optional
    // kind/was_promotion fields sit in the struct -- see those dedicated
    // tests below for events that do set them.
    return ArrivalEvent{.moved_piece = moved,
                        .source = source,
                        .destination = destination,
                        .captured_piece = captured,
                        .arrived_at_ms = arrived_at_ms};
}
}  // namespace

// --- captured_a_king (shared by GameEngine and GameOverObserver) ---

TEST(CapturedAKingTest, FalseWhenNothingWasCaptured) {
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 4});

    EXPECT_FALSE(captured_a_king(make_event(white_rook, Position{4, 5}, Position{4, 4})));
}

TEST(CapturedAKingTest, FalseWhenTheCapturedPieceIsNotAKing) {
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 4});
    Piece black_pawn = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 4});

    EXPECT_FALSE(captured_a_king(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn)));
}

TEST(CapturedAKingTest, TrueWhenTheCapturedPieceIsAKing) {
    Piece black_queen = make_piece(PieceKind::Queen, 1, PieceColor::Black, Position{4, 5});
    Piece white_king = make_piece(PieceKind::King, 2, PieceColor::White, Position{4, 4});

    EXPECT_TRUE(captured_a_king(make_event(black_queen, Position{4, 5}, Position{4, 4}, white_king)));
}

// --- MoveLogObserver ---

TEST(MoveLogObserverTest, StartsEmptyForBothSides) {
    MoveLogObserver log;

    EXPECT_TRUE(log.moves(PieceColor::White).empty());
    EXPECT_TRUE(log.moves(PieceColor::Black).empty());
}

TEST(MoveLogObserverTest, RecordsAnArrivalUnderTheMoversOwnColorOnly) {
    MoveLogObserver log;
    Piece white_pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, Position{4, 4});

    log.on_arrival(make_event(white_pawn, Position{6, 4}, Position{4, 4}));

    EXPECT_EQ(log.moves(PieceColor::White).size(), 1u);
    EXPECT_TRUE(log.moves(PieceColor::Black).empty());
}

TEST(MoveLogObserverTest, EntryNamesThePieceAndBothCells) {
    MoveLogObserver log;
    Piece white_pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, Position{4, 4});

    log.on_arrival(make_event(white_pawn, Position{6, 4}, Position{4, 4}));

    ASSERT_EQ(log.moves(PieceColor::White).size(), 1u);
    const std::string& entry = log.moves(PieceColor::White).front();
    EXPECT_NE(entry.find("wP"), std::string::npos);
    EXPECT_NE(entry.find(to_string(Position{6, 4})), std::string::npos);
    EXPECT_NE(entry.find(to_string(Position{4, 4})), std::string::npos);
}

TEST(MoveLogObserverTest, EntryNotesACaptureButAnOrdinaryMoveEntryDoesNot) {
    MoveLogObserver log;
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 6});
    Piece black_pawn = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 4});

    log.on_arrival(make_event(white_rook, Position{4, 6}, Position{4, 5}));
    log.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn));

    ASSERT_EQ(log.moves(PieceColor::White).size(), 2u);
    EXPECT_EQ(log.moves(PieceColor::White)[0].find('x'), std::string::npos);
    EXPECT_NE(log.moves(PieceColor::White)[1].find('x'), std::string::npos);
}

TEST(MoveLogObserverTest, KeepsMovesInArrivalOrder) {
    MoveLogObserver log;
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 6});

    log.on_arrival(make_event(white_rook, Position{4, 6}, Position{4, 5}));
    log.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}));

    ASSERT_EQ(log.moves(PieceColor::White).size(), 2u);
    EXPECT_NE(log.moves(PieceColor::White)[0].find(to_string(Position{4, 5})), std::string::npos);
    EXPECT_NE(log.moves(PieceColor::White)[1].find(to_string(Position{4, 4})), std::string::npos);
}

TEST(MoveLogObserverTest, EntriesCarryTheArrivalsTimestamp) {
    MoveLogObserver log;
    Piece white_pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, Position{4, 4});

    log.on_arrival(make_event(white_pawn, Position{6, 4}, Position{4, 4}, std::nullopt, /*arrived_at_ms=*/1234));

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().time_ms, 1234);
}

TEST(MoveLogObserverTest, AnOrdinaryPawnMoveNotationIsJustTheDestinationSquare) {
    MoveLogObserver log;  // default board_height 8
    Piece white_pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, Position{6, 4});

    log.on_arrival(make_event(white_pawn, Position{6, 4}, Position{4, 4}));

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().notation, "e4");
}

TEST(MoveLogObserverTest, ANonPawnMoveNotationIsPrefixedByItsPieceLetter) {
    MoveLogObserver log;
    Piece white_knight = make_piece(PieceKind::Knight, 1, PieceColor::White, Position{7, 1});

    log.on_arrival(make_event(white_knight, Position{7, 1}, Position{5, 2}));

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().notation, "Nc3");
}

TEST(MoveLogObserverTest, ANonPawnCaptureNotationHasAnXBeforeTheDestination) {
    MoveLogObserver log;
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 5});
    Piece black_pawn = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 4});

    log.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn));

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().notation, "Rxe4");
}

TEST(MoveLogObserverTest, APawnCaptureNotationIsPrefixedByItsSourceFileInsteadOfAPieceLetter) {
    MoveLogObserver log;
    Piece white_pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, Position{4, 5});
    Piece black_piece = make_piece(PieceKind::Knight, 2, PieceColor::Black, Position{4, 4});

    log.on_arrival(make_event(white_pawn, Position{4, 5}, Position{4, 4}, black_piece));

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().notation, "fxe4");
}

TEST(MoveLogObserverTest, AJumpInPlaceIsMarkedAndNotWrittenAsAMove) {
    MoveLogObserver log;  // e4 == Position{4, 4} on a height-8 board
    Piece white_knight = make_piece(PieceKind::Knight, 1, PieceColor::White, Position{4, 4});

    // A jump: source == destination, kind == JumpInPlace.
    log.on_arrival(ArrivalEvent{.moved_piece = white_knight,
                                .source = Position{4, 4},
                                .destination = Position{4, 4},
                                .kind = MotionKind::JumpInPlace});

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().notation, "Ne4(J)");
    EXPECT_NE(log.moves(PieceColor::White).front().find("(jump)"), std::string::npos);
}

TEST(MoveLogObserverTest, APromotionIsWrittenAsDestinationEqualsQueenNotAQueenMove) {
    MoveLogObserver log;  // e8 == Position{0, 4} on a height-8 board
    // moved_piece already reflects the post-promotion queen (as it does in a
    // real ArrivalEvent), but was_promotion is what makes it "e8=Q" not "Qe8".
    Piece promoted = make_piece(PieceKind::Queen, 1, PieceColor::White, Position{0, 4});

    log.on_arrival(ArrivalEvent{.moved_piece = promoted,
                                .source = Position{1, 4},
                                .destination = Position{0, 4},
                                .was_promotion = true});

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().notation, "e8=Q");
}

TEST(MoveLogObserverTest, ACapturingPromotionKeepsBothTheCaptureFileAndTheEqualsQueen) {
    MoveLogObserver log;
    Piece promoted = make_piece(PieceKind::Queen, 1, PieceColor::White, Position{0, 3});   // d8
    Piece captured = make_piece(PieceKind::Rook, 2, PieceColor::Black, Position{0, 3});

    log.on_arrival(ArrivalEvent{.moved_piece = promoted,
                                .source = Position{1, 4},                                   // from the e-file
                                .destination = Position{0, 3},
                                .captured_piece = captured,
                                .was_promotion = true});

    ASSERT_EQ(log.entries(PieceColor::White).size(), 1u);
    EXPECT_EQ(log.entries(PieceColor::White).front().notation, "exd8=Q");
}

// --- ScoreObserver ---

TEST(ScoreObserverTest, StartsAtZeroForBothSides) {
    ScoreObserver score;

    EXPECT_EQ(score.score(PieceColor::White), 0);
    EXPECT_EQ(score.score(PieceColor::Black), 0);
}

TEST(ScoreObserverTest, AnArrivalWithNoCaptureDoesNotChangeAnyScore) {
    ScoreObserver score;
    Piece white_pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, Position{4, 4});

    score.on_arrival(make_event(white_pawn, Position{6, 4}, Position{4, 4}));

    EXPECT_EQ(score.score(PieceColor::White), 0);
    EXPECT_EQ(score.score(PieceColor::Black), 0);
}

TEST(ScoreObserverTest, CreditsTheMoversColorNotTheCapturedPiecesColor) {
    ScoreObserver score;
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 5});
    Piece black_pawn = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 4});

    score.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn));

    EXPECT_GT(score.score(PieceColor::White), 0);
    EXPECT_EQ(score.score(PieceColor::Black), 0);
}

TEST(ScoreObserverTest, CapturingAQueenIsWorthMoreThanCapturingAPawn) {
    ScoreObserver score_after_pawn;
    ScoreObserver score_after_queen;
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 5});
    Piece black_pawn = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 4});
    Piece black_queen = make_piece(PieceKind::Queen, 3, PieceColor::Black, Position{4, 4});

    score_after_pawn.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn));
    score_after_queen.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_queen));

    EXPECT_GT(score_after_queen.score(PieceColor::White), score_after_pawn.score(PieceColor::White));
}

TEST(ScoreObserverTest, ScoreAccumulatesAcrossMultipleCaptures) {
    ScoreObserver score;
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 6});
    Piece black_pawn_a = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 5});
    Piece black_pawn_b = make_piece(PieceKind::Pawn, 3, PieceColor::Black, Position{4, 4});

    score.on_arrival(make_event(white_rook, Position{4, 6}, Position{4, 5}, black_pawn_a));
    score.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn_b));

    EXPECT_EQ(score.score(PieceColor::White), 2);
}

TEST(ScoreObserverTest, UsesAnInjectedValueProviderInsteadOfTheStandardValues) {
    // A custom provider that values every kind at 100 -- proves the values
    // are data ScoreObserver reads, not a constant baked into it.
    struct FlatValueProvider : IPieceValueProvider {
        int value_of(PieceKind) const override { return 100; }
    };
    FlatValueProvider values;
    ScoreObserver score(values);
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 5});
    Piece black_pawn = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 4});

    score.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn));

    EXPECT_EQ(score.score(PieceColor::White), 100);  // not the standard pawn value of 1
}

// --- GameOverObserver ---

TEST(GameOverObserverTest, StartsNotOver) {
    GameOverObserver game_over;

    EXPECT_FALSE(game_over.is_game_over());
    EXPECT_FALSE(game_over.winner().has_value());
}

TEST(GameOverObserverTest, AnArrivalThatCapturesAnOrdinaryPieceDoesNotEndTheGame) {
    GameOverObserver game_over;
    Piece white_rook = make_piece(PieceKind::Rook, 1, PieceColor::White, Position{4, 5});
    Piece black_pawn = make_piece(PieceKind::Pawn, 2, PieceColor::Black, Position{4, 4});

    game_over.on_arrival(make_event(white_rook, Position{4, 5}, Position{4, 4}, black_pawn));

    EXPECT_FALSE(game_over.is_game_over());
}

TEST(GameOverObserverTest, CapturingTheKingEndsTheGameAndCreditsTheCapturersColor) {
    GameOverObserver game_over;
    Piece black_queen = make_piece(PieceKind::Queen, 1, PieceColor::Black, Position{4, 5});
    Piece white_king = make_piece(PieceKind::King, 2, PieceColor::White, Position{4, 4});

    game_over.on_arrival(make_event(black_queen, Position{4, 5}, Position{4, 4}, white_king));

    ASSERT_TRUE(game_over.is_game_over());
    EXPECT_EQ(*game_over.winner(), PieceColor::Black);
}

TEST(GameOverObserverTest, TheWinnerIsNotOverwrittenByALaterArrival) {
    GameOverObserver game_over;
    Piece black_queen = make_piece(PieceKind::Queen, 1, PieceColor::Black, Position{4, 5});
    Piece white_king = make_piece(PieceKind::King, 2, PieceColor::White, Position{4, 4});
    Piece white_rook = make_piece(PieceKind::Rook, 3, PieceColor::White, Position{0, 0});

    game_over.on_arrival(make_event(black_queen, Position{4, 5}, Position{4, 4}, white_king));
    game_over.on_arrival(make_event(white_rook, Position{0, 0}, Position{0, 1}));

    EXPECT_EQ(*game_over.winner(), PieceColor::Black);
}

TEST(GameOverObserverTest, BothKingsCapturedInTheSameBatchIsADraw) {
    GameOverObserver game_over;
    Piece black_queen = make_piece(PieceKind::Queen, 1, PieceColor::Black, Position{4, 5});
    Piece white_king = make_piece(PieceKind::King, 2, PieceColor::White, Position{4, 4});
    Piece white_queen = make_piece(PieceKind::Queen, 3, PieceColor::White, Position{0, 1});
    Piece black_king = make_piece(PieceKind::King, 4, PieceColor::Black, Position{0, 0});

    // Both arrivals came back in the same RealTimeArbiter::advance_time
    // batch -- neither piece's move was a reaction to the other's arrival.
    game_over.on_arrival(make_event(black_queen, Position{4, 5}, Position{4, 4}, white_king));
    game_over.on_arrival(make_event(white_queen, Position{0, 1}, Position{0, 0}, black_king));

    ASSERT_TRUE(game_over.is_game_over());
    EXPECT_TRUE(game_over.is_draw());
    EXPECT_FALSE(game_over.winner().has_value());
}

TEST(GameOverObserverTest, KingsCapturedAtDifferentTimestampsInTheSameBatchIsNotADraw) {
    GameOverObserver game_over;
    Piece black_queen = make_piece(PieceKind::Queen, 1, PieceColor::Black, Position{4, 5});
    Piece white_king = make_piece(PieceKind::King, 2, PieceColor::White, Position{4, 4});
    Piece white_queen = make_piece(PieceKind::Queen, 3, PieceColor::White, Position{0, 1});
    Piece black_king = make_piece(PieceKind::King, 4, PieceColor::Black, Position{0, 0});

    // Both events came back from the same advance_time call (a single
    // coarse wait() can sweep past multiple arrivals), but at different
    // simulated instants -- black's capture at 600ms already decided the
    // game before white's motion (arriving later, at 1000ms) resolves.
    game_over.on_arrival(make_event(black_queen, Position{4, 5}, Position{4, 4}, white_king, /*arrived_at_ms=*/600));
    game_over.on_arrival(make_event(white_queen, Position{0, 1}, Position{0, 0}, black_king, /*arrived_at_ms=*/1000));

    ASSERT_TRUE(game_over.is_game_over());
    EXPECT_FALSE(game_over.is_draw());
    ASSERT_TRUE(game_over.winner().has_value());
    EXPECT_EQ(*game_over.winner(), PieceColor::Black);
}

TEST(GameOverObserverTest, ADrawIsNotOverwrittenByALaterArrival) {
    GameOverObserver game_over;
    Piece black_queen = make_piece(PieceKind::Queen, 1, PieceColor::Black, Position{4, 5});
    Piece white_king = make_piece(PieceKind::King, 2, PieceColor::White, Position{4, 4});
    Piece white_queen = make_piece(PieceKind::Queen, 3, PieceColor::White, Position{0, 1});
    Piece black_king = make_piece(PieceKind::King, 4, PieceColor::Black, Position{0, 0});
    Piece white_rook = make_piece(PieceKind::Rook, 5, PieceColor::White, Position{2, 2});

    game_over.on_arrival(make_event(black_queen, Position{4, 5}, Position{4, 4}, white_king));
    game_over.on_arrival(make_event(white_queen, Position{0, 1}, Position{0, 0}, black_king));
    game_over.on_arrival(make_event(white_rook, Position{2, 2}, Position{2, 3}));

    EXPECT_TRUE(game_over.is_draw());
    EXPECT_FALSE(game_over.winner().has_value());
}
