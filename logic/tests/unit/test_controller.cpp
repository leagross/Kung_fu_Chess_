#include <gtest/gtest.h>
#include <memory>
#include <optional>

#include "kfc/model/board.hpp"
#include "kfc/engine/game_engine.hpp"
#include "kfc/input/board_mapper.hpp"
#include "kfc/input/controller.hpp"
#include "kfc/realtime/jump_cooldown_policy.hpp"
#include "kfc/realtime/motion_factory.hpp"
#include "kfc/realtime/real_time_arbiter.hpp"
#include "kfc/realtime/standard_cooldown_policy.hpp"
#include "kfc/rules/rule_engine.hpp"
#include "kfc/rules/piece_rule_registry.hpp"
#include "kfc/rules/rook_rule.hpp"

using namespace kfc::model;
using kfc::input::ClickOutcome;
using kfc::input::Controller;
using kfc::input::ControllerResult;

namespace {

Piece make_rook(int id, PieceColor color, Position cell) {
    return Piece{PieceId{id}, color, PieceKind::Rook, cell, PieceState::Idle};
}

PieceRuleRegistry make_registry_with_rook() {
    PieceRuleRegistry registry;
    registry.register_rule(PieceKind::Rook, std::make_unique<RookRule>());
    return registry;
}

// Bundles every collaborator Controller needs, mirroring the GameEngine test
// Harness so each test only has to build a board and add pieces to it.
struct Harness {
    Board board;
    PieceRuleRegistry registry;
    RuleEngine rule_engine;
    RealTimeArbiter arbiter;
    StandardCooldownPolicy standard_policy;
    JumpCooldownPolicy jump_policy;
    MotionFactory motion_factory;
    GameEngine engine;
    Controller controller;

    Harness(int width, int height, std::optional<PieceColor> controlled_color = std::nullopt)
        : board(width, height),
          registry(make_registry_with_rook()),
          rule_engine(registry),
          arbiter(board),
          motion_factory(standard_policy, jump_policy),
          engine(board, rule_engine, arbiter, motion_factory),
          controller(board, engine, kfc::input::BoardMapper(width, height), controlled_color) {}
};

// Pixel at the centre of cell {row, col} (kCellSizePixels == 100).
int cell_x(int col) { return col * 100 + 50; }
int cell_y(int row) { return row * 100 + 50; }

}  // namespace

TEST(ControllerTest, FirstClickOnAnOccupiedCellSelectsIt) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));

    ControllerResult result = h.controller.click(350, 250);

    EXPECT_EQ(result.outcome, ClickOutcome::Selected);
    ASSERT_TRUE(h.controller.selected_cell().has_value());
    Position selected = *h.controller.selected_cell();
    Position expected{2, 3};
    EXPECT_EQ(selected, expected);
}

TEST(ControllerTest, FirstClickOnAnEmptyCellLeavesSelectionEmpty) {
    Harness h(8, 8);

    ControllerResult result = h.controller.click(50, 50);

    EXPECT_EQ(result.outcome, ClickOutcome::Ignored);
    EXPECT_FALSE(h.controller.selected_cell().has_value());
}

TEST(ControllerTest, ClickOutsideTheBoardWithNoSelectionIsIgnored) {
    Harness h(8, 8);

    ControllerResult result = h.controller.click(9000, 9000);

    EXPECT_EQ(result.outcome, ClickOutcome::Ignored);
    EXPECT_FALSE(h.controller.selected_cell().has_value());
}

TEST(ControllerTest, ClickOutsideTheBoardWithASelectionCancelsItWithoutCallingTheEngine) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));
    h.controller.click(350, 250);

    ControllerResult result = h.controller.click(9000, 9000);

    EXPECT_EQ(result.outcome, ClickOutcome::SelectionCleared);
    EXPECT_FALSE(result.move_result.has_value());
    EXPECT_FALSE(h.controller.selected_cell().has_value());
}

TEST(ControllerTest, SecondClickInsideTheBoardSendsTheCorrectSourceAndDestination) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));
    h.controller.click(350, 250);

    ControllerResult result = h.controller.click(350, 450);

    ASSERT_EQ(result.outcome, ClickOutcome::MoveRequested);
    ASSERT_TRUE(result.move_result.has_value());
    EXPECT_TRUE(result.move_result->is_accepted);
    EXPECT_EQ(result.move_result->reason, "ok");
}

TEST(ControllerTest, SecondClickInsideTheBoardClearsSelectionEvenWhenTheMoveIsIllegal) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));
    h.controller.click(350, 250);

    ControllerResult result = h.controller.click(450, 350);  // diagonal: illegal for a rook

    ASSERT_EQ(result.outcome, ClickOutcome::MoveRequested);
    ASSERT_TRUE(result.move_result.has_value());
    EXPECT_FALSE(result.move_result->is_accepted);
    EXPECT_FALSE(h.controller.selected_cell().has_value());
}

TEST(ControllerTest, SecondClickOnAFriendlyPieceReplacesSelectionInsteadOfRequestingAMove) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));
    h.board.add_piece(make_rook(2, PieceColor::White, Position{2, 5}));
    h.controller.click(350, 250);  // selects (2, 3)

    ControllerResult result = h.controller.click(550, 250);  // (2, 5): another white piece

    EXPECT_EQ(result.outcome, ClickOutcome::Selected);
    EXPECT_FALSE(result.move_result.has_value());
    ASSERT_TRUE(h.controller.selected_cell().has_value());
    Position expected{2, 5};
    EXPECT_EQ(*h.controller.selected_cell(), expected);
}

TEST(ControllerTest, SecondClickOnAnEnemyPieceStillRequestsACapture) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));
    h.board.add_piece(Piece{PieceId{2}, PieceColor::Black, PieceKind::Rook, Position{2, 5}, PieceState::Idle});
    h.controller.click(350, 250);  // selects (2, 3)

    ControllerResult result = h.controller.click(550, 250);  // (2, 5): an enemy piece

    ASSERT_EQ(result.outcome, ClickOutcome::MoveRequested);
    ASSERT_TRUE(result.move_result.has_value());
    EXPECT_TRUE(result.move_result->is_accepted);
    EXPECT_FALSE(h.controller.selected_cell().has_value());
}

TEST(ControllerTest, AStaleSelectionWhoseOriginalPieceMovedAwayIsNotUsedToMoveTheNewOccupant) {
    Harness h(8, 8);
    Position originalCell{2, 3};
    Position awayCell{2, 6};
    h.board.add_piece(make_rook(1, PieceColor::White, originalCell));
    h.board.add_piece(make_rook(2, PieceColor::White, awayCell));
    h.controller.click(350, 250);  // selects piece 1 at (2, 3)

    // Piece 1 moves away from (2, 3) (bypassing the controller, as if
    // commanded some other way), then piece 2 slides in to take its place
    // -- the controller's selection still points at cell (2, 3), but the
    // piece that was actually selected is no longer the one sitting there.
    h.engine.request_move(originalCell, Position{5, 3});
    h.engine.wait(5000);
    h.engine.request_move(awayCell, originalCell);
    h.engine.wait(5000);
    ASSERT_TRUE(h.board.piece_at(originalCell).has_value());
    EXPECT_EQ(h.board.piece_at(originalCell)->id, PieceId{2});

    ControllerResult result = h.controller.click(50, 50);  // an empty cell elsewhere

    // A live selection would have requested a move from (2, 3); a stale
    // one is discarded first, so this click is treated as a fresh first
    // click on an empty cell instead.
    EXPECT_EQ(result.outcome, ClickOutcome::Ignored);
    EXPECT_FALSE(result.move_result.has_value());
    EXPECT_FALSE(h.controller.selected_cell().has_value());
}

TEST(ControllerTest, JumpOnAnOccupiedCellRequestsAJump) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));

    ControllerResult result = h.controller.jump(350, 250);

    ASSERT_EQ(result.outcome, ClickOutcome::JumpRequested);
    ASSERT_TRUE(result.move_result.has_value());
    EXPECT_TRUE(result.move_result->is_accepted);
    EXPECT_EQ(result.move_result->reason, "ok");
}

TEST(ControllerTest, JumpOnAnEmptyCellIsRejectedAsEmptySource) {
    Harness h(8, 8);

    ControllerResult result = h.controller.jump(50, 50);

    ASSERT_EQ(result.outcome, ClickOutcome::JumpRequested);
    ASSERT_TRUE(result.move_result.has_value());
    EXPECT_FALSE(result.move_result->is_accepted);
    EXPECT_EQ(result.move_result->reason, "empty_source");
}

TEST(ControllerTest, JumpOutsideTheBoardIsIgnored) {
    Harness h(8, 8);

    ControllerResult result = h.controller.jump(9000, 9000);

    EXPECT_EQ(result.outcome, ClickOutcome::Ignored);
    EXPECT_FALSE(result.move_result.has_value());
}

TEST(ControllerTest, JumpDoesNotDisturbAnInProgressClickSelection) {
    Harness h(8, 8);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));
    h.board.add_piece(make_rook(2, PieceColor::White, Position{5, 5}));
    h.controller.click(350, 250);  // selects (2, 3)

    h.controller.jump(550, 550);  // unrelated piece at (5, 5)

    ASSERT_TRUE(h.controller.selected_cell().has_value());
    Position stillSelected = *h.controller.selected_cell();
    Position expected{2, 3};
    EXPECT_EQ(stillSelected, expected);
}

// --- controlled_color: a networked client only picks up its own pieces ---

TEST(ControllerTest, AColorControlledClientIgnoresAClickOnAnOpponentPiece) {
    Harness h(8, 8, PieceColor::White);
    h.board.add_piece(make_rook(1, PieceColor::Black, Position{2, 3}));

    ControllerResult result = h.controller.click(cell_x(3), cell_y(2));

    EXPECT_EQ(result.outcome, ClickOutcome::Ignored);
    EXPECT_FALSE(h.controller.selected_cell().has_value());
}

TEST(ControllerTest, AColorControlledClientStillSelectsItsOwnPiece) {
    Harness h(8, 8, PieceColor::White);
    h.board.add_piece(make_rook(1, PieceColor::White, Position{2, 3}));

    ControllerResult result = h.controller.click(cell_x(3), cell_y(2));

    EXPECT_EQ(result.outcome, ClickOutcome::Selected);
    ASSERT_TRUE(h.controller.selected_cell().has_value());
    EXPECT_EQ(*h.controller.selected_cell(), (Position{2, 3}));
}

TEST(ControllerTest, WithNoControlledColorEitherSideCanBeSelectedForLocalHotSeatPlay) {
    Harness h(8, 8);  // no controlled color -> local play
    h.board.add_piece(make_rook(1, PieceColor::Black, Position{2, 3}));

    ControllerResult result = h.controller.click(cell_x(3), cell_y(2));

    EXPECT_EQ(result.outcome, ClickOutcome::Selected);  // black is selectable in local play
}
