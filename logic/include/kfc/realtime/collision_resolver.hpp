#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// What a mover finds at its destination; decided without touching Board.
enum class CollisionKind {
    VacatedCell,          // empty, or (for a jump-in-place) held only the mover
    EnemyCaptured,
    FriendlyBlocked,
    PassedThroughAirborne,  // occupant is mid-jump, not really there yet
};

struct CollisionResult {
    CollisionKind kind;
    std::optional<Piece> captured_piece;  // set only when kind == EnemyCaptured
};

/// Decides what one mover finds at one destination cell.
class CollisionResolver {
public:
    [[nodiscard]] static CollisionResult resolve(const Piece& mover, const std::optional<Piece>& occupant);
};

}  // namespace kfc::model
