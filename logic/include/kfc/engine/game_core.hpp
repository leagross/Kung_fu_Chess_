#pragma once

#include <utility>

#include "../../kfc/engine/game_engine.hpp"
#include "../../kfc/model/board.hpp"
#include "../../kfc/realtime/cooldown_policy.hpp"
#include "../../kfc/realtime/motion_factory.hpp"
#include "../../kfc/realtime/piece_speed_provider.hpp"
#include "../../kfc/realtime/real_time_arbiter.hpp"
#include "../../kfc/rules/piece_rule_registry.hpp"
#include "../../kfc/rules/rule_engine.hpp"

namespace kfc::model {

/// Composition root of one game's core simulation, wired identically for local and networked play.
class GameCore {
public:
    /// standard_policy, jump_policy, and speed_provider must outlive this GameCore.
    GameCore(Board board, const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
             const IPieceSpeedProvider& speed_provider = kDefaultPieceSpeedProvider,
             double meters_per_cell = kDefaultMetersPerCell);

    /// Members hold references into each other, so move/copy would leave dangling references.
    GameCore(const GameCore&) = delete;
    GameCore& operator=(const GameCore&) = delete;
    GameCore(GameCore&&) = delete;
    GameCore& operator=(GameCore&&) = delete;

    Board& board() { return board_; }
    const Board& board() const { return board_; }

    GameEngine& engine() { return engine_; }
    const GameEngine& engine() const { return engine_; }

    RealTimeArbiter& arbiter() { return arbiter_; }
    const RealTimeArbiter& arbiter() const { return arbiter_; }

private:
    // Declaration order is construction order; do not reorder.
    Board board_;
    PieceRuleRegistry registry_;
    RuleEngine rule_engine_;
    RealTimeArbiter arbiter_;
    MotionFactory motion_factory_;
    GameEngine engine_;
};

}  // namespace kfc::model
