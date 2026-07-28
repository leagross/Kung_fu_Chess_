#pragma once

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// Strategy interface: how fast (in meters per second) a piece kind moves in
/// its "move" state. MotionFactory depends on this instead of a hardcoded
/// number so where that number comes from -- a fixed placeholder for tests,
/// or a real per-piece config.json read by the graphics layer -- is never
/// MotionFactory's concern. Kept in kfc::realtime (backend), even though the
/// real config-reading implementation lives in kfc::graphics (frontend):
/// this is dependency inversion, the same reason IMovementRule lives here
/// and not inside whatever eventually renders a piece.
class IPieceSpeedProvider {
public:
    virtual ~IPieceSpeedProvider() = default;

    /// Meters per second this piece kind moves in its "move" state.
    [[nodiscard]] virtual double speed_m_per_sec(PieceKind kind) const = 0;
};

/// A speed provider with no dependency on config files -- what every
/// existing MotionFactory caller (all current tests, and Game unless told
/// otherwise) gets by default. Reproduces the exact numbers MotionFactory
/// used to hardcode directly: 1.5 m/s for every kind except Drone, which
/// gets 1.0 m/s (paired with the default 1.5 meters-per-cell, this
/// reproduces the old 1000ms/1500ms-per-cell-step values exactly).
class FixedPieceSpeedProvider : public IPieceSpeedProvider {
public:
    double speed_m_per_sec(PieceKind kind) const override;
};

}  // namespace kfc::model
