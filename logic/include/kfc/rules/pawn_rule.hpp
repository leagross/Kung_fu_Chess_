#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Pawn movement: one cell straight forward onto an empty cell, or one cell
/// diagonally forward onto an enemy piece (capture only -- a pawn never
/// moves diagonally onto an empty cell). "Forward" means decreasing row for
/// White and increasing row for Black.
///
/// A pawn that has never moved (Piece::has_moved is false) may also move
/// two cells straight forward, but only if both the one-cell and two-cell
/// destinations are empty -- the path must be clear, not just the landing
/// square. Eligibility is tracked per-piece, not derived from row/board
/// height, so it works the same on an 8x8 board and on the small ad-hoc
/// boards this project's fixtures use. No en passant. Promotion is not this
/// rule's concern: it happens on arrival, in RealTimeArbiter.
class PawnRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
