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

/// Composition root of one game's core simulation:
/// Board -> PieceRuleRegistry -> RuleEngine -> RealTimeArbiter ->
/// MotionFactory -> GameEngine, wired identically for local and networked
/// play. Each host layers its own concern on top (Game adds a Controller
/// and observers; Match adds a tick thread and command queue).
///
/// Cooldown policies and speed provider are injected, not owned -- they
/// must outlive this GameCore. Owns the Board itself, since the arbiter
/// must hold the one and only mutable reference to it.
class GameCore {
public:
    /// board is moved in and becomes the live board. standard_policy,
    /// jump_policy, and speed_provider must all outlive this GameCore.
    GameCore(Board board, const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
             const IPieceSpeedProvider& speed_provider = kDefaultPieceSpeedProvider,
             double meters_per_cell = kDefaultMetersPerCell);

    /// Neither copyable nor movable: members hold references to each other
    /// (RuleEngine to registry_, arbiter_/engine_ to board_), so a
    /// compiler-generated move would leave the moved-from object's
    /// references dangling once it goes out of scope.
    GameCore(const GameCore&) = delete;
    GameCore& operator=(const GameCore&) = delete;
    GameCore(GameCore&&) = delete;
    GameCore& operator=(GameCore&&) = delete;

    /// The live board. Non-const overload exists for symmetry with the
    /// arbiter's own mutable access; read-only callers should take the
    /// const one.
    Board& board() { return board_; }
    const Board& board() const { return board_; }

    /// The command boundary (request_move/request_jump/wait/is_game_over).
    GameEngine& engine() { return engine_; }
    const GameEngine& engine() const { return engine_; }

    /// In-flight motion / cooldown / busy state -- for a renderer or a host
    /// that needs to read a just-started Motion back out (see Match).
    RealTimeArbiter& arbiter() { return arbiter_; }
    const RealTimeArbiter& arbiter() const { return arbiter_; }

private:
    // Declaration order is construction order; each member depends on the
    // ones before it -- do not reorder.
    Board board_;
    PieceRuleRegistry registry_;
    RuleEngine rule_engine_;
    RealTimeArbiter arbiter_;
    MotionFactory motion_factory_;
    GameEngine engine_;
};

}  // namespace kfc::model
