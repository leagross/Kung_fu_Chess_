#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Pawn movement: one cell straight forward onto an empty cell, or one cell
/// diagonally forward onto an enemy piece (capture only). "Forward" is
/// decreasing row for White, increasing for Black. A pawn that has never
/// moved may also step two cells forward if both cells are empty. No en
/// passant; promotion happens on arrival, in RealTimeArbiter.
class PawnRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
