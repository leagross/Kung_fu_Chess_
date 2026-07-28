#include <gtest/gtest.h>
#include "kfc/model/piece.hpp"

using namespace kfc::model;

TEST(PieceTest, StoresIdColorKindAndInitialCell) {
    Position start{0, 0};
    Piece p{PieceId{1}, PieceColor::White, PieceKind::Rook, start, PieceState::Idle};

    EXPECT_EQ(p.id, PieceId{1});
    EXPECT_EQ(p.color, PieceColor::White);
    EXPECT_EQ(p.kind, PieceKind::Rook);
    EXPECT_EQ(p.cell, start);
    EXPECT_EQ(p.state, PieceState::Idle);
}

TEST(PieceTest, StateCanTransitionToMoving) {
    Position start{0, 0};
    Piece p{PieceId{1}, PieceColor::White, PieceKind::Rook, start, PieceState::Idle};

    p.state = PieceState::Moving;

    EXPECT_EQ(p.state, PieceState::Moving);
}

TEST(PieceTest, StateCanTransitionToCaptured) {
    Position start{0, 0};
    Piece p{PieceId{1}, PieceColor::White, PieceKind::Rook, start, PieceState::Idle};

    p.state = PieceState::Captured;

    EXPECT_EQ(p.state, PieceState::Captured);
}

TEST(PieceIdTest, DifferentValuesAreNotEqual) {
    PieceId a{1};
    PieceId b{2};

    EXPECT_NE(a, b);
}
