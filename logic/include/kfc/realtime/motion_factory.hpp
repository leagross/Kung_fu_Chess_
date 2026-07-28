#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/cooldown_policy.hpp"
#include "../../kfc/realtime/motion.hpp"
#include "../../kfc/realtime/piece_speed_provider.hpp"

namespace kfc::model {

/// The default speed_provider/meters_per_cell MotionFactory falls back to
/// when a caller doesn't supply its own -- reproduces the exact 1000ms (or
/// 1500ms for Drone) per cell-step every existing caller/test already
/// depends on: FixedPieceSpeedProvider's 1.5 m/s (1.0 for Drone) divided
/// into kDefaultMetersPerCell's 1.5 meters is 1000ms (1500ms) per step,
/// unchanged from before this class read speed from anywhere.
extern const FixedPieceSpeedProvider kDefaultPieceSpeedProvider;
inline constexpr double kDefaultMetersPerCell = 1.5;

/// Builds fully-formed Motion objects, deciding duration and cooldown so
/// that RealTimeArbiter never has to. An ordinary move's duration is
/// distance-based: cell-steps * meters_per_cell / speed_provider's
/// m/s for that piece kind; a jump-in-place's duration is fixed, since its
/// distance is always zero.
class MotionFactory {
public:
    /// standard_policy, jump_policy, and speed_provider must all outlive
    /// this MotionFactory. meters_per_cell converts a piece's configured
    /// speed (in meters/second) into a per-cell-step duration; it is a
    /// rendering/feel choice, not something MotionFactory can derive on its
    /// own, so callers who care about pacing (e.g. the GUI app) pass their
    /// own instead of accepting the default.
    MotionFactory(const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
                  const IPieceSpeedProvider& speed_provider = kDefaultPieceSpeedProvider,
                  double meters_per_cell = kDefaultMetersPerCell);

    /// An ordinary move from source to destination.
    [[nodiscard]] Motion create_move(const Piece& piece, const Position& source, const Position& destination) const;

    /// A jump-in-place: destination equals cell. Duration is fixed at
    /// 700ms (see kJumpDurationMs in the .cpp) -- long enough for a human to
    /// notice an incoming attack and double-click to jump in response; tune
    /// freely.
    [[nodiscard]] Motion create_jump(const Piece& piece, const Position& cell) const;

private:
    const ICooldownPolicy& standard_policy_;
    const ICooldownPolicy& jump_policy_;
    const IPieceSpeedProvider& speed_provider_;
    double meters_per_cell_;
};

}  // namespace kfc::model
