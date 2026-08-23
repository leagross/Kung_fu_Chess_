#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// RookRule and BishopRule combined: slides in all eight directions until blocked.
class QueenRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
