#pragma once

#include <vector>

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Strategy interface for a single piece kind's movement geometry. Answers
/// "where could this piece go", nothing else -- never mutates anything.
class IMovementRule {
public:
    virtual ~IMovementRule() = default;

    /// All cells piece could legally move to. Enemy-occupied cells that
    /// could be captured are included; friendly-occupied cells are not.
    [[nodiscard]] virtual std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const = 0;
};

}  // namespace kfc::model
