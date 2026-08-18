#pragma once

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// Strategy interface: how fast (m/s) a piece kind moves in its "move"
/// state. MotionFactory depends on this instead of a hardcoded number so a
/// real per-piece config.json can replace the fixed test default later.
class IPieceSpeedProvider {
public:
    virtual ~IPieceSpeedProvider() = default;

    /// Meters per second this piece kind moves in its "move" state.
    [[nodiscard]] virtual double speed_m_per_sec(PieceKind kind) const = 0;
};

/// The default with no config-file dependency: 1.5 m/s for every kind
/// except Drone (1.0 m/s).
class FixedPieceSpeedProvider : public IPieceSpeedProvider {
public:
    double speed_m_per_sec(PieceKind kind) const override;
};

}  // namespace kfc::model
