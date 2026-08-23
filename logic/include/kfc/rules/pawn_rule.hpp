#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Straight forward onto empty, diagonal onto enemy (capture only), or two forward if never moved.
class PawnRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
