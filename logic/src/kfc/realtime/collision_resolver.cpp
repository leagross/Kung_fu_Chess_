#include "../../../include/kfc/realtime/collision_resolver.hpp"

namespace kfc::model {

CollisionResult CollisionResolver::resolve(const Piece& mover, const std::optional<Piece>& occupant) {
    if (!occupant.has_value() || occupant->id == mover.id) {
        return CollisionResult{CollisionKind::VacatedCell, std::nullopt};
    }
    // A friendly piece blocks the mover whether it is idle or mid-jump: the
    // mover never displaces or passes an ally. This color check MUST come
    // before the Airborne check below -- pass-through exists only so an
    // *enemy* attacker cannot capture a defender in the air (the defender
    // lands and resolves its own arrival). Letting a *friendly* airborne
    // occupant fall through to pass-through instead corrupts the board: the
    // mover takes the cell, the jumper's board record is cleared, and when
    // the jumper lands it finds the ally now sitting there, is FriendlyBlocked,
    // and -- its cell already gone -- is never placed back anywhere, silently
    // vanishing. See JumpFriendlyBlockTest.
    if (occupant->color == mover.color) {
        return CollisionResult{CollisionKind::FriendlyBlocked, std::nullopt};
    }
    if (occupant->state == PieceState::Airborne) {
        return CollisionResult{CollisionKind::PassedThroughAirborne, std::nullopt};
    }
    Piece captured = *occupant;
    captured.state = PieceState::Captured;
    return CollisionResult{CollisionKind::EnemyCaptured, captured};
}

}  // namespace kfc::model
