#include <gtest/gtest.h>

#include "kfc/model/board.hpp"
#include "kfc/rules/bishop_rule.hpp"
#include "kfc/rules/king_rule.hpp"
#include "kfc/rules/knight_rule.hpp"
#include "kfc/rules/pawn_rule.hpp"
#include "kfc/rules/queen_rule.hpp"

using namespace kfc::model;

namespace {

Piece make_piece(PieceKind kind, int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, kind, cell, PieceState::Idle};
}

bool contains(const std::vector<Position>& destinations, Position target) {
    for (const Position& destination : destinations) {
        if (destination == target) {
            return true;
        }
    }
    return false;
}

}  // namespace

// --- BishopRule ---

TEST(BishopRuleTest, MovesDiagonallyNotStraight) {
    Board board(8, 8);
    Position start{4, 4};
    Piece bishop = make_piece(PieceKind::Bishop, 1, PieceColor::White, start);
    board.add_piece(bishop);
    BishopRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, bishop);

    EXPECT_TRUE(contains(destinations, Position{6, 6}));
    EXPECT_FALSE(contains(destinations, Position{4, 6}));
    EXPECT_FALSE(contains(destinations, Position{6, 4}));
}

TEST(BishopRuleTest, StopsBeforeAFriendlyBlockerOnTheDiagonal) {
    Board board(8, 8);
    Position start{4, 4};
    Position blocker{6, 6};
    Piece bishop = make_piece(PieceKind::Bishop, 1, PieceColor::White, start);
    board.add_piece(bishop);
    board.add_piece(make_piece(PieceKind::Bishop, 2, PieceColor::White, blocker));
    BishopRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, bishop);

    EXPECT_TRUE(contains(destinations, Position{5, 5}));
    EXPECT_FALSE(contains(destinations, Position{6, 6}));
    EXPECT_FALSE(contains(destinations, Position{7, 7}));
}

// --- QueenRule ---

TEST(QueenRuleTest, CombinesRookAndBishopMovement) {
    Board board(8, 8);
    Position start{4, 4};
    Piece queen = make_piece(PieceKind::Queen, 1, PieceColor::White, start);
    board.add_piece(queen);
    QueenRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, queen);

    EXPECT_TRUE(contains(destinations, Position{4, 7}));
    EXPECT_TRUE(contains(destinations, Position{0, 4}));
    EXPECT_TRUE(contains(destinations, Position{7, 7}));
}

// --- KnightRule ---

TEST(KnightRuleTest, JumpsInAnLShape) {
    Board board(8, 8);
    Position start{4, 4};
    Piece knight = make_piece(PieceKind::Knight, 1, PieceColor::White, start);
    board.add_piece(knight);
    KnightRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, knight);

    EXPECT_TRUE(contains(destinations, Position{2, 3}));
    EXPECT_TRUE(contains(destinations, Position{6, 5}));
    EXPECT_FALSE(contains(destinations, Position{4, 5}));
}

TEST(KnightRuleTest, JumpsOverBlockersOnAllAdjacentCells) {
    Board board(8, 8);
    Position start{4, 4};
    Piece knight = make_piece(PieceKind::Knight, 1, PieceColor::White, start);
    board.add_piece(knight);
    for (int row_offset = -1; row_offset <= 1; ++row_offset) {
        for (int col_offset = -1; col_offset <= 1; ++col_offset) {
            if (row_offset == 0 && col_offset == 0) {
                continue;
            }
            Position adjacent{start.row + row_offset, start.col + col_offset};
            board.add_piece(make_piece(PieceKind::Pawn, 100 + row_offset * 10 + col_offset,
                                        PieceColor::White, adjacent));
        }
    }
    KnightRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, knight);

    EXPECT_TRUE(contains(destinations, Position{2, 3}));
}

// --- KingRule ---

TEST(KingRuleTest, MovesExactlyOneCellInEveryDirection) {
    Board board(8, 8);
    Position start{4, 4};
    Piece king = make_piece(PieceKind::King, 1, PieceColor::White, start);
    board.add_piece(king);
    KingRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, king);

    EXPECT_TRUE(contains(destinations, Position{3, 4}));
    EXPECT_TRUE(contains(destinations, Position{5, 5}));
    EXPECT_FALSE(contains(destinations, Position{2, 4}));
    EXPECT_FALSE(contains(destinations, Position{6, 6}));
}

// --- PawnRule ---

TEST(PawnRuleTest, WhitePawnMovesOneCellTowardDecreasingRow) {
    Board board(8, 8);
    Position start{6, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    board.add_piece(pawn);
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_TRUE(contains(destinations, Position{5, 4}));
    EXPECT_FALSE(contains(destinations, Position{7, 4}));
}

TEST(PawnRuleTest, BlackPawnMovesOneCellTowardIncreasingRow) {
    Board board(8, 8);
    Position start{1, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::Black, start);
    board.add_piece(pawn);
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_TRUE(contains(destinations, Position{2, 4}));
    EXPECT_FALSE(contains(destinations, Position{0, 4}));
}

TEST(PawnRuleTest, CannotMoveStraightForwardOntoAnOccupiedCell) {
    Board board(8, 8);
    Position start{6, 4};
    Position blocked{5, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    board.add_piece(pawn);
    board.add_piece(make_piece(PieceKind::Pawn, 2, PieceColor::Black, blocked));
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_FALSE(contains(destinations, blocked));
}

TEST(PawnRuleTest, CapturesDiagonallyOntoAnEnemyPiece) {
    Board board(8, 8);
    Position start{6, 4};
    Position enemy{5, 5};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    board.add_piece(pawn);
    board.add_piece(make_piece(PieceKind::Pawn, 2, PieceColor::Black, enemy));
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_TRUE(contains(destinations, enemy));
}

TEST(PawnRuleTest, CannotMoveDiagonallyOntoAnEmptyCell) {
    Board board(8, 8);
    Position start{6, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    board.add_piece(pawn);
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_FALSE(contains(destinations, Position{5, 5}));
    EXPECT_FALSE(contains(destinations, Position{5, 3}));
}

TEST(PawnRuleTest, CannotMoveTwoCellsOnceThePieceHasAlreadyMoved) {
    Board board(8, 8);
    Position start{6, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    pawn.has_moved = true;
    board.add_piece(pawn);
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_FALSE(contains(destinations, Position{4, 4}));
}

TEST(PawnRuleTest, WhitePawnHasATwoCellOpeningMoveWhenItHasNotMovedYet) {
    Board board(8, 8);
    Position start{7, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    board.add_piece(pawn);
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_TRUE(contains(destinations, Position{5, 4}));
}

TEST(PawnRuleTest, BlackPawnHasATwoCellOpeningMoveWhenItHasNotMovedYet) {
    Board board(8, 8);
    Position start{0, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::Black, start);
    board.add_piece(pawn);
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_TRUE(contains(destinations, Position{2, 4}));
}

TEST(PawnRuleTest, TheTwoCellOpeningMoveIsAvailableFromAnyRowNotJustTheTraditionalOne) {
    // Eligibility comes from has_moved, not from board height, so this
    // works even on a row a standard 8x8 game would never place a pawn on
    // -- exactly the small ad-hoc boards this project's fixtures use.
    Board board(8, 8);
    Position start{4, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    board.add_piece(pawn);
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_TRUE(contains(destinations, Position{2, 4}));
}

TEST(PawnRuleTest, TwoCellOpeningMoveIsBlockedIfThePathIsNotClear) {
    Board board(8, 8);
    Position start{7, 4};
    Position blocker{6, 4};
    Piece pawn = make_piece(PieceKind::Pawn, 1, PieceColor::White, start);
    board.add_piece(pawn);
    board.add_piece(make_piece(PieceKind::Pawn, 2, PieceColor::Black, blocker));
    PawnRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, pawn);

    EXPECT_FALSE(contains(destinations, Position{5, 4}));
}
