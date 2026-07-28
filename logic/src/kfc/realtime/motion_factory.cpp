#include "../../../include/kfc/realtime/motion_factory.hpp"

#include <algorithm>
#include <cmath>

namespace kfc::model {

const FixedPieceSpeedProvider kDefaultPieceSpeedProvider;

namespace {
// How long a jump-in-place keeps its piece Airborne (see CollisionResolver
// and RealTimeArbiter::resolve_arrival for what that protects against: an
// attacker arriving mid-jump passes through instead of capturing, and the
// defender's own landing moments later captures the attacker back). 300ms
// proved too short for a human to react to and double-click within once an
// incoming attack is noticed; widened to give a real chance to respond.
constexpr int kJumpDurationMs = 700;

int cell_step_distance(const Position& source, const Position& destination) {
    int row_delta = std::abs(destination.row - source.row);
    int col_delta = std::abs(destination.col - source.col);
    return std::max(row_delta, col_delta);
}
}  // namespace

MotionFactory::MotionFactory(const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
                              const IPieceSpeedProvider& speed_provider, double meters_per_cell)
    : standard_policy_(standard_policy),
      jump_policy_(jump_policy),
      speed_provider_(speed_provider),
      meters_per_cell_(meters_per_cell) {}

Motion MotionFactory::create_move(const Piece& piece, const Position& source, const Position& destination) const {
    int steps = cell_step_distance(source, destination);
    double speed = speed_provider_.speed_m_per_sec(piece.kind);
    int duration_ms = speed > 0.0 ? static_cast<int>(steps * meters_per_cell_ / speed * 1000.0) : 0;
    return Motion{piece, source, destination, MotionKind::Move, duration_ms, 0, standard_policy_.cooldown_ms()};
}

Motion MotionFactory::create_jump(const Piece& piece, const Position& cell) const {
    return Motion{piece, cell, cell, MotionKind::JumpInPlace, kJumpDurationMs, 0, jump_policy_.cooldown_ms()};
}

}  // namespace kfc::model
