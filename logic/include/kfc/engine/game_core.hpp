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

/// The shared composition root of one game's core simulation:
/// Board -> PieceRuleRegistry -> RuleEngine -> RealTimeArbiter ->
/// MotionFactory -> GameEngine, wired identically for local play
/// (kfc::texttests::Game) and networked play (kfc::server::Match). Before
/// this class existed, both assembled that exact same five-object chain by
/// hand, so any change to the wiring (e.g. injecting a promotion or
/// win-condition policy) had to be made in two places and kept in sync.
/// Now each host owns one GameCore and layers only its own distinct concern
/// on top: Game adds a Controller and an observer list; Match adds a tick
/// thread and a command queue.
///
/// The cooldown policies and speed provider are injected, not owned -- they
/// must outlive this GameCore -- so each host decides where those come from
/// (the fixed backend constants, or the GUI's config.json-driven ones)
/// without GameCore having to care. Owns the Board itself, since the arbiter
/// inside must hold the one and only mutable reference to it.
class GameCore {
public:
    /// board is moved in and becomes the live board. standard_policy,
    /// jump_policy, and speed_provider must all outlive this GameCore --
    /// they are forwarded straight into the internal MotionFactory (see its
    /// own constructor for what meters_per_cell means and why these default
    /// the way they do).
    GameCore(Board board, const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
             const IPieceSpeedProvider& speed_provider = kDefaultPieceSpeedProvider,
             double meters_per_cell = kDefaultMetersPerCell);

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
    // Declaration order is construction order, and every member below
    // depends on the ones before it (rule_engine_ on registry_, arbiter_ on
    // board_, motion_factory_ on the injected policies, engine_ on all of
    // them) -- do not reorder.
    Board board_;
    PieceRuleRegistry registry_;
    RuleEngine rule_engine_;
    RealTimeArbiter arbiter_;
    MotionFactory motion_factory_;
    GameEngine engine_;
};

}  // namespace kfc::model
