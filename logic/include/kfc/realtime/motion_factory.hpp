#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/cooldown_policy.hpp"
#include "../../kfc/realtime/motion.hpp"
#include "../../kfc/realtime/piece_speed_provider.hpp"

namespace kfc::model {

/// Default speed_provider/meters_per_cell: reproduces the 1000ms (1500ms
/// for Drone) per cell-step every existing caller/test already depends on.
extern const FixedPieceSpeedProvider kDefaultPieceSpeedProvider;
inline constexpr double kDefaultMetersPerCell = 1.5;

/// Builds fully-formed Motion objects, deciding duration and cooldown so
/// RealTimeArbiter never has to. An ordinary move's duration is distance-based
/// (cell-steps * meters_per_cell / speed); a jump-in-place's is fixed.
class MotionFactory {
public:
    /// standard_policy, jump_policy, and speed_provider must all outlive
    /// this MotionFactory.
    MotionFactory(const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
                  const IPieceSpeedProvider& speed_provider = kDefaultPieceSpeedProvider,
                  double meters_per_cell = kDefaultMetersPerCell);

    /// An ordinary move from source to destination.
    [[nodiscard]] Motion create_move(const Piece& piece, const Position& source, const Position& destination) const;

    /// A jump-in-place: destination equals cell, duration fixed (kJumpDurationMs).
    [[nodiscard]] Motion create_jump(const Piece& piece, const Position& cell) const;

private:
    const ICooldownPolicy& standard_policy_;
    const ICooldownPolicy& jump_policy_;
    const IPieceSpeedProvider& speed_provider_;
    double meters_per_cell_;
};

}  // namespace kfc::model
