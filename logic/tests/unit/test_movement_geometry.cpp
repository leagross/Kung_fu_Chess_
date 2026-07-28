#include <gtest/gtest.h>

#include "kfc/model/board.hpp"
#include "kfc/rules/movement_geometry.hpp"

using namespace kfc::model;

namespace {

Piece make_piece(PieceColor color, Position cell) {
    return Piece{PieceId{1}, color, PieceKind::Rook, cell, PieceState::Idle};
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

// --- sliding_destinations ---

TEST(SlidingDestinationsTest, WalksToTheBoardEdgeInEachGivenDirection) {
    Board board(8, 8);
    Position start{4, 4};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    std::vector<std::pair<int, int>> directions = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    std::vector<Position> destinations = sliding_destinations(board, piece, directions);

    EXPECT_TRUE(contains(destinations, Position{0, 4}));
    EXPECT_TRUE(contains(destinations, Position{7, 4}));
    EXPECT_TRUE(contains(destinations, Position{4, 0}));
    EXPECT_TRUE(contains(destinations, Position{4, 7}));
}

TEST(SlidingDestinationsTest, CombinesDestinationsFromEveryDirectionGiven) {
    Board board(8, 8);
    Position start{4, 4};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    std::vector<std::pair<int, int>> directions = {{-1, -1}, {1, 1}};  // one diagonal, both ways

    std::vector<Position> destinations = sliding_destinations(board, piece, directions);

    EXPECT_TRUE(contains(destinations, Position{3, 3}));
    EXPECT_TRUE(contains(destinations, Position{5, 5}));
    EXPECT_FALSE(contains(destinations, Position{4, 5}));  // not one of the given directions
}

TEST(SlidingDestinationsTest, StopsBeforeAFriendlyPieceExcludingIt) {
    Board board(8, 8);
    Position start{4, 4};
    Position blocker{4, 6};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    board.add_piece(make_piece(PieceColor::White, blocker));
    std::vector<std::pair<int, int>> directions = {{0, 1}};

    std::vector<Position> destinations = sliding_destinations(board, piece, directions);

    EXPECT_TRUE(contains(destinations, Position{4, 5}));
    EXPECT_FALSE(contains(destinations, blocker));
    EXPECT_FALSE(contains(destinations, Position{4, 7}));
}

TEST(SlidingDestinationsTest, IncludesAnEnemyPieceThenStopsPastIt) {
    Board board(8, 8);
    Position start{4, 4};
    Position blocker{4, 6};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    board.add_piece(make_piece(PieceColor::Black, blocker));
    std::vector<std::pair<int, int>> directions = {{0, 1}};

    std::vector<Position> destinations = sliding_destinations(board, piece, directions);

    EXPECT_TRUE(contains(destinations, blocker));
    EXPECT_FALSE(contains(destinations, Position{4, 7}));
}

TEST(SlidingDestinationsTest, NeverWalksPastTheBoardEdge) {
    Board board(3, 3);
    Position start{0, 0};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    std::vector<std::pair<int, int>> directions = {{-1, 0}, {0, -1}};  // both point off the board

    std::vector<Position> destinations = sliding_destinations(board, piece, directions);

    EXPECT_TRUE(destinations.empty());
}

TEST(SlidingDestinationsTest, ReturnsNothingWhenNoDirectionsAreGiven) {
    Board board(8, 8);
    Position start{4, 4};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);

    std::vector<Position> destinations = sliding_destinations(board, piece, {});

    EXPECT_TRUE(destinations.empty());
}

// --- stepping_destinations ---

TEST(SteppingDestinationsTest, IncludesEveryInBoundsOffset) {
    Board board(8, 8);
    Position start{4, 4};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    std::vector<std::pair<int, int>> offsets = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};

    std::vector<Position> destinations = stepping_destinations(board, piece, offsets);

    EXPECT_EQ(destinations.size(), 4u);
    EXPECT_TRUE(contains(destinations, Position{3, 4}));
    EXPECT_TRUE(contains(destinations, Position{5, 4}));
    EXPECT_TRUE(contains(destinations, Position{4, 3}));
    EXPECT_TRUE(contains(destinations, Position{4, 5}));
}

TEST(SteppingDestinationsTest, SkipsOffsetsThatFallOutsideTheBoard) {
    Board board(8, 8);
    Position start{0, 0};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    std::vector<std::pair<int, int>> offsets = {{-1, 0}, {0, -1}, {1, 0}};

    std::vector<Position> destinations = stepping_destinations(board, piece, offsets);

    EXPECT_EQ(destinations.size(), 1u);
    EXPECT_TRUE(contains(destinations, Position{1, 0}));
}

TEST(SteppingDestinationsTest, ExcludesAnOffsetOccupiedByAFriendlyPiece) {
    Board board(8, 8);
    Position start{4, 4};
    Position friendlyCell{5, 4};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    board.add_piece(make_piece(PieceColor::White, friendlyCell));
    std::vector<std::pair<int, int>> offsets = {{1, 0}};

    std::vector<Position> destinations = stepping_destinations(board, piece, offsets);

    EXPECT_TRUE(destinations.empty());
}

TEST(SteppingDestinationsTest, IncludesAnOffsetOccupiedByAnEnemyPiece) {
    Board board(8, 8);
    Position start{4, 4};
    Position enemyCell{5, 4};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);
    board.add_piece(make_piece(PieceColor::Black, enemyCell));
    std::vector<std::pair<int, int>> offsets = {{1, 0}};

    std::vector<Position> destinations = stepping_destinations(board, piece, offsets);

    EXPECT_TRUE(contains(destinations, enemyCell));
}

TEST(SteppingDestinationsTest, ReturnsNothingWhenNoOffsetsAreGiven) {
    Board board(8, 8);
    Position start{4, 4};
    Piece piece = make_piece(PieceColor::White, start);
    board.add_piece(piece);

    std::vector<Position> destinations = stepping_destinations(board, piece, {});

    EXPECT_TRUE(destinations.empty());
}
