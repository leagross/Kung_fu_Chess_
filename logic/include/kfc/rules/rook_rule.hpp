#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Slides horizontally/vertically until blocked by the board edge, a friendly piece, or a captured enemy.
class RookRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
