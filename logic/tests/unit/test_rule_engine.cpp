#include <gtest/gtest.h>
#include <memory>

#include "kfc/model/board.hpp"
#include "kfc/rules/rule_engine.hpp"
#include "kfc/rules/piece_rule_registry.hpp"
#include "kfc/rules/rook_rule.hpp"

using namespace kfc::model;

namespace {

Piece make_rook(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::Rook, cell, PieceState::Idle};
}

PieceRuleRegistry make_registry_with_rook() {
    PieceRuleRegistry registry;
    registry.register_rule(PieceKind::Rook, std::make_unique<RookRule>());
    return registry;
}

}  // namespace

TEST(RuleEngineTest, AcceptsALegalRookMove) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 7};
    board.add_piece(make_rook(1, PieceColor::White, source));
    PieceRuleRegistry registry = make_registry_with_rook();
    RuleEngine engine(registry);

    MoveValidation result = engine.validate_move(board, source, destination);

    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.reason, "ok");
}

TEST(RuleEngineTest, RejectsAMoveFromAnEmptySource) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 7};
    PieceRuleRegistry registry = make_registry_with_rook();
    RuleEngine engine(registry);

    MoveValidation result = engine.validate_move(board, source, destination);

    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.reason, "empty_source");
}

TEST(RuleEngineTest, RejectsADestinationOutsideTheBoard) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{8, 4};
    board.add_piece(make_rook(1, PieceColor::White, source));
    PieceRuleRegistry registry = make_registry_with_rook();
    RuleEngine engine(registry);

    MoveValidation result = engine.validate_move(board, source, destination);

    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.reason, "outside_board");
}

TEST(RuleEngineTest, RejectsAMoveOntoAFriendlyPiece) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 6};
    board.add_piece(make_rook(1, PieceColor::White, source));
    board.add_piece(make_rook(2, PieceColor::White, destination));
    PieceRuleRegistry registry = make_registry_with_rook();
    RuleEngine engine(registry);

    MoveValidation result = engine.validate_move(board, source, destination);

    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.reason, "friendly_destination");
}

TEST(RuleEngineTest, RejectsAMoveTheRuleDoesNotAllow) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{5, 5};
    board.add_piece(make_rook(1, PieceColor::White, source));
    PieceRuleRegistry registry = make_registry_with_rook();
    RuleEngine engine(registry);

    MoveValidation result = engine.validate_move(board, source, destination);

    EXPECT_FALSE(result.is_valid);
    EXPECT_EQ(result.reason, "illegal_piece_move");
}

TEST(RuleEngineTest, AcceptsAMoveThatCapturesAnEnemyPiece) {
    Board board(8, 8);
    Position source{4, 4};
    Position destination{4, 6};
    board.add_piece(make_rook(1, PieceColor::White, source));
    board.add_piece(make_rook(2, PieceColor::Black, destination));
    PieceRuleRegistry registry = make_registry_with_rook();
    RuleEngine engine(registry);

    MoveValidation result = engine.validate_move(board, source, destination);

    EXPECT_TRUE(result.is_valid);
    EXPECT_EQ(result.reason, "ok");
}
