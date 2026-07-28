#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"

namespace kfc::model {

/// What a mover finds waiting for it at its destination cell, decided
/// without touching Board -- RealTimeArbiter is the only thing that
/// actually mutates the board, this just classifies the situation so it
/// knows what to do. A pure decision, easy to test on its own without
/// wiring up a Board or a Motion.
enum class CollisionKind {
    /// Destination was empty, or (for a jump-in-place) held only the mover
    /// itself -- an ordinary, uneventful arrival.
    VacatedCell,
    /// An enemy piece is there -- it gets captured.
    EnemyCaptured,
    /// A friendly piece is there -- the mover never displaces an ally; it
    /// is blocked and stays at its own source cell instead.
    FriendlyBlocked,
    /// The occupant is mid-jump (PieceState::Airborne) -- it isn't really
    /// there yet, friend or foe, so the mover passes through uneventfully.
    /// Whoever now sits in that cell is whatever the jumper finds when it
    /// lands and resolves its own arrival.
    PassedThroughAirborne,
};

struct CollisionResult {
    CollisionKind kind;
    /// Set only when kind == EnemyCaptured -- the occupant's snapshot,
    /// already marked PieceState::Captured.
    std::optional<Piece> captured_piece;
};

/// Decides the outcome of one mover reaching one destination cell: an
/// empty/vacated cell, an enemy piece (capture), a friendly piece (blocked),
/// an occupant mid-jump (passed through), and a jump-in-place landing back
/// on itself (whose destination equals its own source, so it always finds
/// only itself there -- handled by the same occupant-identity check as any
/// other "cell already held by the mover" case, no special-casing needed).
class CollisionResolver {
public:
    [[nodiscard]] static CollisionResult resolve(const Piece& mover, const std::optional<Piece>& occupant);
};

}  // namespace kfc::model
