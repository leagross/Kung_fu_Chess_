#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// What a mover finds waiting for it at its destination cell, decided
/// without touching Board -- only RealTimeArbiter actually mutates it.
enum class CollisionKind {
    /// Destination was empty, or (for a jump-in-place) held only the mover.
    VacatedCell,
    /// An enemy piece is there -- it gets captured.
    EnemyCaptured,
    /// A friendly piece is there -- the mover is blocked and stays put.
    FriendlyBlocked,
    /// The occupant is mid-jump (PieceState::Airborne) and isn't really
    /// there yet, so the mover passes through uneventfully.
    PassedThroughAirborne,
};

struct CollisionResult {
    CollisionKind kind;
    /// Set only when kind == EnemyCaptured, already marked PieceState::Captured.
    std::optional<Piece> captured_piece;
};

/// Decides what one mover finds at one destination cell.
class CollisionResolver {
public:
    [[nodiscard]] static CollisionResult resolve(const Piece& mover, const std::optional<Piece>& occupant);
};

}  // namespace kfc::model
