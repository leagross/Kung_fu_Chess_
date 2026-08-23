#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// L-shape jump; never blocked, only stopped by the board edge or a friendly piece on landing.
class KnightRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
