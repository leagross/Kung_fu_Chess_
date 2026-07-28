#pragma once

#include <vector>

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Strategy interface for a single piece kind's movement geometry. A rule
/// answers "where could this piece go on this board", nothing else -- it
/// never captures, removes, moves, or otherwise mutates anything.
class IMovementRule {
public:
    virtual ~IMovementRule() = default;

    /// All cells piece could legally move to on board. Enemy-occupied cells
    /// that could be captured are included; friendly-occupied cells are not.
    /// [[nodiscard]] on the interface covers every concrete rule called
    /// through it (RuleEngine only ever calls via IMovementRule&).
    [[nodiscard]] virtual std::vector<Position> legal_destinations(const Board& board, const Piece& piece) const = 0;
};

}  // namespace kfc::model
