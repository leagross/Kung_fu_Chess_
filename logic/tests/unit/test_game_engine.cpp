#include <gtest/gtest.h>
#include <memory>

#include "kfc/model/board.hpp"
#include "kfc/engine/game_engine.hpp"
#include "kfc/realtime/jump_cooldown_policy.hpp"
#include "kfc/realtime/motion_factory.hpp"
#include "kfc/realtime/real_time_arbiter.hpp"
#include "kfc/realtime/standard_cooldown_policy.hpp"
#include "kfc/rules/rule_engine.hpp"
#include "kfc/rules/piece_rule_registry.hpp"
#include "kfc/rules/rook_rule.hpp"

using namespace kfc::model;

namespace {

Piece make_rook(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::Rook, cell, PieceState::Idle};
}

Piece make_king(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::King, cell, PieceState::Idle};
}

PieceRuleRegistry make_registry_with_rook() {
    PieceRuleRegistry registry;
    registry.register_rule(PieceKind::Rook, std::make_unique<RookRule>());
    return registry;
}

// Bundles every collaborator GameEngine needs so each test only has to
// build a board and add pieces to it, instead of re-wiring five objects.
struct Harness {
    Board board;
    PieceRuleRegistry registry;
    RuleEngine rule_engine;
    RealTimeArbiter arbiter;
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory motion_factory;
    GameEngine engine;

    Harness(int width, int height)
        : board(width, height),
          registry(make_registry_with_rook()),
          rule_engine(registry),
          arbiter(board),
          motion_factory(standard_policy, jump_policy),
          engine(board, rule_engine, arbiter, motion_factory) {}
};

}  // namespace

TEST(GameEngineTest, AcceptsALegalMoveAndDelegatesToRuleEngine) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 7};
    h.board.add_piece(make_rook(1, PieceColor::White, source));

    MoveResult result = h.engine.request_move(source, destination);

    EXPECT_TRUE(result.is_accepted);
    EXPECT_EQ(result.reason, "ok");
}

TEST(GameEngineTest, RejectsAnIllegalMoveWithTheRuleEnginesReason) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{5, 5};
    h.board.add_piece(make_rook(1, PieceColor::White, source));

    MoveResult result = h.engine.request_move(source, destination);

    EXPECT_FALSE(result.is_accepted);
    EXPECT_EQ(result.reason, "illegal_piece_move");
}

TEST(GameEngineTest, LeavesTheBoardUnchangedImmediatelyAfterARequestBeforeAnyWait) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 7};
    h.board.add_piece(make_rook(1, PieceColor::White, source));

    h.engine.request_move(source, destination);

    EXPECT_TRUE(h.board.piece_at(source).has_value());
    EXPECT_FALSE(h.board.piece_at(destination).has_value());
}

TEST(GameEngineTest, MovesThePieceOnceWaitCoversTheFullDuration) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 6};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.engine.request_move(source, destination);

    h.engine.wait(2000);

    EXPECT_FALSE(h.board.piece_at(source).has_value());
    EXPECT_TRUE(h.board.piece_at(destination).has_value());
}

TEST(GameEngineTest, RejectsASecondMoveOfTheSamePieceWhileItIsStillInProgress) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 6};
    Position furtherDestination{4, 7};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.engine.request_move(source, destination);

    MoveResult result = h.engine.request_move(source, furtherDestination);

    EXPECT_FALSE(result.is_accepted);
    EXPECT_EQ(result.reason, "motion_in_progress");
}

TEST(GameEngineTest, AcceptsAnUnrelatedPiecesMoveWhileAnotherPieceIsStillInProgress) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 6};
    Position otherSource{0, 0};
    Position otherDestination{0, 1};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.board.add_piece(make_rook(2, PieceColor::White, otherSource));
    h.engine.request_move(source, destination);

    MoveResult result = h.engine.request_move(otherSource, otherDestination);

    EXPECT_TRUE(result.is_accepted);
    EXPECT_EQ(result.reason, "ok");
}

TEST(GameEngineTest, RejectsANewMoveOfTheSamePieceWhileItIsStillRestingAfterArrival) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Position nextDestination{4, 6};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.engine.request_move(source, destination);
    h.engine.wait(1000);  // exactly the arrival time -- rest cooldown has not started counting down yet

    MoveResult result = h.engine.request_move(destination, nextDestination);

    EXPECT_FALSE(result.is_accepted);
    EXPECT_EQ(result.reason, "motion_in_progress");
}

TEST(GameEngineTest, AcceptsANewMoveOnceTheRestCooldownHasFullyElapsed) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Position nextDestination{4, 6};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.engine.request_move(source, destination);
    h.engine.wait(1000);                     // arrival
    h.engine.wait(h.standard_policy.cooldown_ms());  // rest cooldown fully elapses

    MoveResult result = h.engine.request_move(destination, nextDestination);

    EXPECT_TRUE(result.is_accepted);
    EXPECT_EQ(result.reason, "ok");
}

TEST(GameEngineTest, CapturingANonKingPieceDoesNotEndTheGame) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.board.add_piece(make_rook(2, PieceColor::Black, destination));
    h.engine.request_move(source, destination);

    h.engine.wait(1000);

    EXPECT_FALSE(h.engine.is_game_over());
    ASSERT_TRUE(h.board.piece_at(destination).has_value());
    EXPECT_EQ(h.board.piece_at(destination)->id, PieceId{1});
}

TEST(GameEngineTest, CapturingTheEnemyKingEndsTheGame) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.board.add_piece(make_king(2, PieceColor::Black, destination));
    h.engine.request_move(source, destination);

    h.engine.wait(1000);

    EXPECT_TRUE(h.engine.is_game_over());
}

TEST(GameEngineTest, StopsAdvancingAnyInFlightMotionOnceTheGameHasEnded) {
    Harness h(8, 8);
    Position attackerSource{4, 4};
    Position kingCell{4, 5};
    Position bystanderSource{0, 0};
    Position bystanderDestination{0, 3};
    h.board.add_piece(make_rook(1, PieceColor::White, attackerSource));
    h.board.add_piece(make_king(2, PieceColor::Black, kingCell));
    h.board.add_piece(make_rook(3, PieceColor::White, bystanderSource));
    h.engine.request_move(attackerSource, kingCell);              // arrives at 1000ms, ends the game
    h.engine.request_move(bystanderSource, bystanderDestination);  // unrelated, slower, still in flight

    h.engine.wait(1000);  // attacker arrives, captures the king, game over
    ASSERT_TRUE(h.engine.is_game_over());

    ArrivalEvents laterEvents = h.engine.wait(5000);  // long past the bystander's own arrival time

    EXPECT_TRUE(laterEvents.empty());
    EXPECT_TRUE(h.board.piece_at(bystanderSource).has_value());
    EXPECT_FALSE(h.board.piece_at(bystanderDestination).has_value());
}

TEST(GameEngineTest, RejectsAnyFurtherMoveAfterTheGameHasEnded) {
    Harness h(8, 8);
    Position source{4, 4};
    Position destination{4, 5};
    Position anotherSource{0, 0};
    Position anotherDestination{0, 1};
    h.board.add_piece(make_rook(1, PieceColor::White, source));
    h.board.add_piece(make_king(2, PieceColor::Black, destination));
    h.board.add_piece(make_rook(3, PieceColor::White, anotherSource));
    h.engine.request_move(source, destination);
    h.engine.wait(1000);

    MoveResult result = h.engine.request_move(anotherSource, anotherDestination);

    EXPECT_FALSE(result.is_accepted);
    EXPECT_EQ(result.reason, "game_over");
}
