#include <gtest/gtest.h>

#include "kfc/realtime/pawn_promotion.hpp"

using namespace kfc::model;

namespace {
Piece make_pawn(PieceColor color, Position cell) {
    return Piece{PieceId{1}, color, PieceKind::Pawn, cell, PieceState::Idle};
}
}  // namespace

TEST(PawnPromotionTest, AWhitePawnOnRowZeroBecomesAQueen) {
    Board board(8, 8);
    Piece pawn = make_pawn(PieceColor::White, Position{0, 4});

    apply_pawn_promotion(pawn, board);

    EXPECT_EQ(pawn.kind, PieceKind::Queen);
}

TEST(PawnPromotionTest, ABlackPawnOnTheLastRowBecomesAQueen) {
    Board board(8, 8);
    Piece pawn = make_pawn(PieceColor::Black, Position{7, 4});

    apply_pawn_promotion(pawn, board);

    EXPECT_EQ(pawn.kind, PieceKind::Queen);
}

TEST(PawnPromotionTest, APawnShortOfItsPromotionRowIsUnaffected) {
    Board board(8, 8);
    Piece pawn = make_pawn(PieceColor::White, Position{1, 4});

    apply_pawn_promotion(pawn, board);

    EXPECT_EQ(pawn.kind, PieceKind::Pawn);
}

TEST(PawnPromotionTest, ANonPawnIsNeverAffected) {
    Board board(8, 8);
    Piece rook{PieceId{1}, PieceColor::White, PieceKind::Rook, Position{0, 4}, PieceState::Idle};

    apply_pawn_promotion(rook, board);

    EXPECT_EQ(rook.kind, PieceKind::Rook);
}
