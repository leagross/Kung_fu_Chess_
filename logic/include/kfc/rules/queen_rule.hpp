#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// Rook movement combined with bishop movement -- slides in all eight
/// directions until blocked, exactly like RookRule and BishopRule combined.
class QueenRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
