#include <gtest/gtest.h>
#include <stdexcept>

#include "kfc/model/board.hpp"

using namespace kfc::model;

namespace {

Piece make_piece(int id, Position cell) {
    return Piece{PieceId{id}, PieceColor::White, PieceKind::Rook, cell, PieceState::Idle};
}

}  // namespace

TEST(BoardTest, DimensionsAreDerivedFromConstructor) {
    Board board(8, 5);

    EXPECT_EQ(board.width(), 8);
    EXPECT_EQ(board.height(), 5);
}

TEST(BoardTest, PositionInsideBoardIsInBounds) {
    Board board(8, 8);
    Position inside{3, 3};

    EXPECT_TRUE(board.in_bounds(inside));
}

TEST(BoardTest, PositionOutsideBoardIsNotInBounds) {
    Board board(8, 8);
    Position outside{8, 0};

    EXPECT_FALSE(board.in_bounds(outside));
}

TEST(BoardTest, EmptyCellReturnsNoPiece) {
    Board board(8, 8);
    Position empty{2, 2};

    EXPECT_FALSE(board.piece_at(empty).has_value());
}

TEST(BoardTest, OccupiedCellReturnsTheCorrectPiece) {
    Board board(8, 8);
    Position cell{2, 2};
    Piece rook = make_piece(1, cell);
    board.add_piece(rook);

    std::optional<Piece> found = board.piece_at(cell);

    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->id, PieceId{1});
}

TEST(BoardTest, AddingTwoPiecesToTheSameCellFails) {
    Board board(8, 8);
    Position cell{2, 2};
    board.add_piece(make_piece(1, cell));

    EXPECT_THROW(board.add_piece(make_piece(2, cell)), std::logic_error);
}

TEST(BoardTest, MovingAPieceUpdatesSourceAndDestination) {
    Board board(8, 8);
    Position from{0, 0};
    Position to{0, 1};
    board.add_piece(make_piece(1, from));

    board.move_piece(from, to);

    EXPECT_FALSE(board.piece_at(from).has_value());
    std::optional<Piece> arrived = board.piece_at(to);
    ASSERT_TRUE(arrived.has_value());
    EXPECT_EQ(arrived->id, PieceId{1});
}

TEST(BoardTest, RemovingACapturedPieceClearsItsCell) {
    Board board(8, 8);
    Position cell{4, 4};
    board.add_piece(make_piece(1, cell));

    board.remove_piece(cell);

    EXPECT_FALSE(board.piece_at(cell).has_value());
}

TEST(BoardTest, SetPieceStateUpdatesStateWithoutTouchingAnythingElse) {
    Board board(8, 8);
    Position cell{4, 4};
    board.add_piece(make_piece(1, cell));

    board.set_piece_state(cell, PieceState::Moving);

    std::optional<Piece> found = board.piece_at(cell);
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(found->state, PieceState::Moving);
    EXPECT_EQ(found->id, PieceId{1});
    EXPECT_EQ(found->cell, cell);
}

// --- Defensive guards: invalid input fails loudly instead of corrupting
// memory (a negative dimension used to wrap to a huge allocation; the
// mutators used to index out of range or dereference an empty cell). ---

TEST(BoardTest, ConstructorRejectsNegativeDimensions) {
    EXPECT_THROW(Board(-1, 8), std::invalid_argument);
    EXPECT_THROW(Board(8, -1), std::invalid_argument);
}

TEST(BoardTest, SetPieceStateThrowsOnAnEmptyCell) {
    Board board(8, 8);
    EXPECT_THROW(board.set_piece_state(Position{4, 4}, PieceState::Moving), std::logic_error);
}

TEST(BoardTest, SetPieceStateThrowsOutOfBounds) {
    Board board(8, 8);
    EXPECT_THROW(board.set_piece_state(Position{8, 0}, PieceState::Moving), std::out_of_range);
}

TEST(BoardTest, RemovePieceThrowsOutOfBounds) {
    Board board(8, 8);
    EXPECT_THROW(board.remove_piece(Position{-1, 0}), std::out_of_range);
}

TEST(BoardTest, MovePieceThrowsWhenTheSourceIsEmpty) {
    Board board(8, 8);
    EXPECT_THROW(board.move_piece(Position{0, 0}, Position{0, 1}), std::logic_error);
}

TEST(BoardTest, MovePieceThrowsOutOfBounds) {
    Board board(8, 8);
    board.add_piece(make_piece(1, Position{0, 0}));
    EXPECT_THROW(board.move_piece(Position{0, 0}, Position{0, 8}), std::out_of_range);
}
