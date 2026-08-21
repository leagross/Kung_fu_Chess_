#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Steps 1 or 2 cells along one cardinal direction, never diagonal --
/// jumps straight to the destination, ignoring what's in between (like a
/// knight). Slower than other pieces (see MotionFactory).
class DroneRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
