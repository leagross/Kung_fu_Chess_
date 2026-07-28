#include <gtest/gtest.h>
#include <stdexcept>

#include "kfc/model/board.hpp"
#include "kfc/rules/piece_rule_registry.hpp"
#include "kfc/rules/rook_rule.hpp"

using namespace kfc::model;

namespace {

Piece make_rook(PieceColor color, Position cell) {
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

TEST(RookRuleTest, MovesFreelyAlongEmptyRowAndColumn) {
    Board board(8, 8);
    Position start{4, 4};
    Piece rook = make_rook(PieceColor::White, start);
    board.add_piece(rook);
    RookRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, rook);

    EXPECT_TRUE(contains(destinations, Position{0, 4}));
    EXPECT_TRUE(contains(destinations, Position{7, 4}));
    EXPECT_TRUE(contains(destinations, Position{4, 0}));
    EXPECT_TRUE(contains(destinations, Position{4, 7}));
}

TEST(RookRuleTest, CannotMoveDiagonally) {
    Board board(8, 8);
    Position start{4, 4};
    Piece rook = make_rook(PieceColor::White, start);
    board.add_piece(rook);
    RookRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, rook);

    EXPECT_FALSE(contains(destinations, Position{5, 5}));
}

TEST(RookRuleTest, StopsBeforeAFriendlyBlocker) {
    Board board(8, 8);
    Position start{4, 4};
    Position blocker{4, 6};
    Piece rook = make_rook(PieceColor::White, start);
    board.add_piece(rook);
    board.add_piece(make_rook(PieceColor::White, blocker));
    RookRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, rook);

    EXPECT_TRUE(contains(destinations, Position{4, 5}));
    EXPECT_FALSE(contains(destinations, Position{4, 6}));
    EXPECT_FALSE(contains(destinations, Position{4, 7}));
}

TEST(RookRuleTest, CapturesAnEnemyBlockerButDoesNotPassIt) {
    Board board(8, 8);
    Position start{4, 4};
    Position blocker{4, 6};
    Piece rook = make_rook(PieceColor::White, start);
    board.add_piece(rook);
    board.add_piece(make_rook(PieceColor::Black, blocker));
    RookRule rule;

    std::vector<Position> destinations = rule.legal_destinations(board, rook);

    EXPECT_TRUE(contains(destinations, Position{4, 6}));
    EXPECT_FALSE(contains(destinations, Position{4, 7}));
}

TEST(PieceRuleRegistryTest, ReturnsTheRuleRegisteredForAKind) {
    PieceRuleRegistry registry;
    registry.register_rule(PieceKind::Rook, std::make_unique<RookRule>());
    Board board(8, 8);
    Position start{4, 4};
    Piece rook = make_rook(PieceColor::White, start);
    board.add_piece(rook);

    const IMovementRule& rule = registry.rule_for(PieceKind::Rook);
    std::vector<Position> destinations = rule.legal_destinations(board, rook);

    EXPECT_TRUE(contains(destinations, Position{4, 7}));
}

TEST(PieceRuleRegistryTest, ThrowsWhenNoRuleIsRegisteredForAKind) {
    PieceRuleRegistry registry;

    EXPECT_THROW(registry.rule_for(PieceKind::Rook), std::out_of_range);
}
