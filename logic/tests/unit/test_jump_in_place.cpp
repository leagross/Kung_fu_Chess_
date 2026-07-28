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

PieceRuleRegistry make_registry_with_rook() {
    PieceRuleRegistry registry;
    registry.register_rule(PieceKind::Rook, std::make_unique<RookRule>());
    return registry;
}

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

// --- CooldownPolicy ---

TEST(CooldownPolicyTest, BothImposeARealCooldownAndTheStandardOneIsLonger) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;

    EXPECT_GT(jump_policy.cooldown_ms(), 0);
    EXPECT_GT(standard_policy.cooldown_ms(), jump_policy.cooldown_ms());
}

// --- MotionFactory ---

TEST(MotionFactoryTest, CreateMoveDurationScalesWithCellSteps) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory factory(standard_policy, jump_policy);
    Piece rook = make_rook(1, PieceColor::White, Position{4, 4});

    Motion motion = factory.create_move(rook, Position{4, 4}, Position{4, 6});

    EXPECT_EQ(motion.duration_ms, 2000);
    EXPECT_EQ(motion.kind, MotionKind::Move);
}

TEST(MotionFactoryTest, CreateMoveUsesTheStandardCooldown) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory factory(standard_policy, jump_policy);
    Piece rook = make_rook(1, PieceColor::White, Position{4, 4});

    Motion motion = factory.create_move(rook, Position{4, 4}, Position{4, 5});

    EXPECT_EQ(motion.cooldown_ms, standard_policy.cooldown_ms());
}

TEST(MotionFactoryTest, CreateJumpHasSourceEqualToDestination) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory factory(standard_policy, jump_policy);
    Piece rook = make_rook(1, PieceColor::White, Position{4, 4});

    Motion motion = factory.create_jump(rook, Position{4, 4});

    EXPECT_EQ(motion.source, motion.destination);
    EXPECT_EQ(motion.kind, MotionKind::JumpInPlace);
}

TEST(MotionFactoryTest, CreateJumpUsesTheJumpCooldownNotTheStandardOne) {
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory factory(standard_policy, jump_policy);
    Piece rook = make_rook(1, PieceColor::White, Position{4, 4});

    Motion motion = factory.create_jump(rook, Position{4, 4});

    EXPECT_EQ(motion.cooldown_ms, jump_policy.cooldown_ms());
}

// --- GameEngine::request_jump ---

TEST(RequestJumpTest, RejectsAnEmptyCell) {
    Harness h(8, 8);

    MoveResult result = h.engine.request_jump(Position{4, 4});

    EXPECT_FALSE(result.is_accepted);
    EXPECT_EQ(result.reason, "empty_source");
}

TEST(RequestJumpTest, AcceptsAJumpForAnIdlePiece) {
    Harness h(8, 8);
    Position cell{4, 4};
    h.board.add_piece(make_rook(1, PieceColor::White, cell));

    MoveResult result = h.engine.request_jump(cell);

    EXPECT_TRUE(result.is_accepted);
    EXPECT_EQ(result.reason, "ok");
}

TEST(RequestJumpTest, RejectsASecondJumpWhileTheFirstIsStillInFlight) {
    Harness h(8, 8);
    Position cell{4, 4};
    h.board.add_piece(make_rook(1, PieceColor::White, cell));
    h.engine.request_jump(cell);

    MoveResult result = h.engine.request_jump(cell);

    EXPECT_FALSE(result.is_accepted);
    EXPECT_EQ(result.reason, "motion_in_progress");
}

// --- The race: the exact two scenarios from the original rule ---
//
// Attacker starts a 2-cell move (2000ms) toward the defender. The defender
// reacts partway through by jumping in place. Both scenarios below only
// differ in *when* the defender jumps relative to the attacker's progress
// -- nothing else changes, and no special-case code exists for either
// outcome. It falls out entirely from RealTimeArbiter processing whichever
// motion's elapsed time reaches its duration first.

TEST(JumpRaceTest, DefenderWinsWhenTheAttackerArrivesWhileTheJumpIsStillAirborne) {
    Harness h(8, 8);
    Position attackerStart{4, 6};
    Position defenderCell{4, 4};
    h.board.add_piece(make_rook(1, PieceColor::White, attackerStart));
    h.board.add_piece(make_rook(2, PieceColor::Black, defenderCell));

    h.engine.request_move(attackerStart, defenderCell);
    h.engine.wait(1900);                      // attacker: 1900/2000, not yet arrived
    h.engine.request_jump(defenderCell);       // defender reacts late, jump: 0/700
    h.engine.wait(100);                        // attacker: 2000/2000 arrives; jump: 100/700, still airborne
    h.engine.wait(600);                        // jump: 700/700 arrives, lands on the attacker

    std::optional<Piece> occupant = h.board.piece_at(defenderCell);
    ASSERT_TRUE(occupant.has_value());
    EXPECT_EQ(occupant->id, PieceId{2});  // the defender is still standing
}

TEST(JumpRaceTest, DefenderWinsTheSameWayRegardlessOfHowCoarselyTimeIsAdvanced) {
    // Same scenario and same total elapsed time as
    // DefenderWinsWhenTheAttackerArrivesWhileTheJumpIsStillAirborne, but the
    // last 700ms arrive as a single wait() instead of two. The outcome must
    // not depend on how the caller chunks its wait() calls.
    Harness h(8, 8);
    Position attackerStart{4, 6};
    Position defenderCell{4, 4};
    h.board.add_piece(make_rook(1, PieceColor::White, attackerStart));
    h.board.add_piece(make_rook(2, PieceColor::Black, defenderCell));

    h.engine.request_move(attackerStart, defenderCell);
    h.engine.wait(1900);                      // attacker: 1900/2000, not yet arrived
    h.engine.request_jump(defenderCell);       // defender reacts late, jump: 0/700
    h.engine.wait(700);                        // attacker arrives mid-batch, then the jump lands

    std::optional<Piece> occupant = h.board.piece_at(defenderCell);
    ASSERT_TRUE(occupant.has_value());
    EXPECT_EQ(occupant->id, PieceId{2});  // the defender is still standing
}

TEST(JumpRaceTest, AttackerWinsWhenTheDefenderHasAlreadyLandedAndIsResting) {
    Harness h(8, 8);
    Position attackerStart{4, 6};
    Position defenderCell{4, 4};
    h.board.add_piece(make_rook(1, PieceColor::White, attackerStart));
    h.board.add_piece(make_rook(2, PieceColor::Black, defenderCell));

    h.engine.request_move(attackerStart, defenderCell);
    h.engine.wait(1000);                      // attacker: 1000/2000, not yet arrived
    h.engine.request_jump(defenderCell);       // defender reacts early, jump: 0/700
    h.engine.wait(700);                        // attacker: 1700/2000; jump: 700/700 arrives, lands safely
    h.engine.wait(300);                        // attacker: 2000/2000 arrives, finds the resting defender

    std::optional<Piece> occupant = h.board.piece_at(defenderCell);
    ASSERT_TRUE(occupant.has_value());
    EXPECT_EQ(occupant->id, PieceId{1});  // the attacker took the cell
}

// --- The friendly-jump corruption this fixes ---
//
// Unlike the enemy race above, a *friendly* piece arriving at a mid-jump
// ally's cell must NOT pass through it: an ally is never displaced. Driven
// through the arbiter directly (the moves below deliberately target an
// occupied cell, which RuleEngine would reject -- the bug lives in arrival
// resolution, not in legality). Before the fix, the arriving ally passed
// through, took the cell, and the jumper -- its board record cleared, then
// FriendlyBlocked on landing -- was never placed back and vanished entirely.
TEST(JumpFriendlyBlockTest, AJumpingPieceIsNotLostWhenAnAllyArrivesAtItsCell) {
    Harness h(8, 8);
    Position jumperCell{4, 4};
    Position allyStart{4, 5};
    h.board.add_piece(make_rook(2, PieceColor::White, jumperCell));
    h.board.add_piece(make_rook(1, PieceColor::White, allyStart));

    // Ally glides toward the jumper's cell (1 cell = 1000ms).
    h.arbiter.start_motion(h.motion_factory.create_move(*h.board.piece_at(allyStart), allyStart, jumperCell));
    h.arbiter.advance_time(400);
    // Jumper leaves the ground at t=400; airborne window is [400, 1100].
    h.arbiter.start_motion(h.motion_factory.create_jump(*h.board.piece_at(jumperCell), jumperCell));
    h.arbiter.advance_time(600);  // t=1000: ally arrives while the jumper is airborne (600/700)
    h.arbiter.advance_time(100);  // t=1100: the jumper lands

    // Both pieces must still exist: the jumper back on its own cell, the ally
    // blocked and still on its start cell -- neither displaced, none vanished.
    std::optional<Piece> jumper = h.board.piece_at(jumperCell);
    std::optional<Piece> ally = h.board.piece_at(allyStart);
    ASSERT_TRUE(jumper.has_value());
    EXPECT_EQ(jumper->id, PieceId{2});
    ASSERT_TRUE(ally.has_value());
    EXPECT_EQ(ally->id, PieceId{1});
}
