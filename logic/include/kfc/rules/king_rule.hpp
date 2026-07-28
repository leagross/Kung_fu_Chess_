#pragma once

#include "../../kfc/rules/movement_rule.hpp"

namespace kfc::model {

/// One cell in any of the eight surrounding directions.
class KingRule : public IMovementRule {
public:
    std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const override;
};

}  // namespace kfc::model
