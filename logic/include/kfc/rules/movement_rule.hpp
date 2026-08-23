#pragma once

#include <vector>

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Strategy interface for one piece kind's movement geometry; never mutates anything.
class IMovementRule {
public:
    virtual ~IMovementRule() = default;

    /// Includes capturable enemy-occupied cells; excludes friendly-occupied ones.
    [[nodiscard]] virtual std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const = 0;
};

}  // namespace kfc::model
