#include "../../../include/kfc/realtime/collision_resolver.hpp"

namespace kfc::model {

CollisionResult CollisionResolver::resolve(const Piece& mover, const std::optional<Piece>& occupant) {
    if (!occupant.has_value() || occupant->id == mover.id) {
        return CollisionResult{CollisionKind::VacatedCell, std::nullopt};
    }
    // Color check must precede the Airborne check: an airborne friendly
    // occupant must still block, not pass-through, or its board record gets
    // cleared and it vanishes on landing (see JumpFriendlyBlockTest).
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
