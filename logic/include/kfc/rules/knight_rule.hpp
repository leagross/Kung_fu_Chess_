#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Jumps in an L-shape (two cells one way, one cell perpendicular),
/// ignoring anything in between -- it is never blocked, only stopped by the
/// board edge or a friendly piece on the landing cell.
class KnightRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
