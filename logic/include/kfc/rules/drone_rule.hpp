#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Steps 1-2 cells along one cardinal direction, jumping straight to the destination like a knight.
class DroneRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
