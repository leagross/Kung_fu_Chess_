#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// One cell straight forward onto an empty cell, or diagonally onto an
/// enemy (capture only); two cells forward if never moved and both are
/// empty. No en passant; promotion happens on arrival, in RealTimeArbiter.
class PawnRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
