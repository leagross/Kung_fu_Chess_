#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Steps 1 or 2 cells along a single cardinal direction (up, down, left, or
/// right) -- never diagonal. Like a knight, it jumps straight to the
/// destination and ignores whatever sits in between; only the destination
/// cell itself matters. Slower than other pieces -- see MotionFactory for
/// the drone's longer per-cell duration.
class DroneRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
