#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/cooldown_policy.hpp"
#include "../../kfc/realtime/motion.hpp"
#include "../../kfc/realtime/piece_speed_provider.hpp"

namespace kfc::model {

/// Reproduces the 1000ms (1500ms for Drone) per cell-step every existing caller/test depends on.
extern const FixedPieceSpeedProvider kDefaultPieceSpeedProvider;
inline constexpr double kDefaultMetersPerCell = 1.5;

/// Builds Motion objects, deciding duration and cooldown so RealTimeArbiter never has to.
class MotionFactory {
public:
    /// standard_policy, jump_policy, and speed_provider must outlive this MotionFactory.
    MotionFactory(const ICooldownPolicy& standard_policy, const ICooldownPolicy& jump_policy,
                  const IPieceSpeedProvider& speed_provider = kDefaultPieceSpeedProvider,
                  double meters_per_cell = kDefaultMetersPerCell);

    [[nodiscard]] Motion create_move(const Piece& piece, const Position& source, const Position& destination) const;

    /// destination equals cell; duration is fixed (kJumpDurationMs).
    [[nodiscard]] Motion create_jump(const Piece& piece, const Position& cell) const;

private:
    const ICooldownPolicy& standard_policy_;
    const ICooldownPolicy& jump_policy_;
    const IPieceSpeedProvider& speed_provider_;
    double meters_per_cell_;
};

}  // namespace kfc::model
