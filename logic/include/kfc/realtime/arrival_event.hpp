#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_kind.hpp"

namespace kfc::model {

/// What happened when one moving piece finished arriving at its destination.
/// moved_piece/captured_piece are full snapshots so observers need not re-query Board afterward.
struct ArrivalEvent {
    Piece moved_piece;
    Position source;
    Position destination;
    std::optional<Piece> captured_piece;
    MotionKind kind = MotionKind::Move;
    bool was_promotion = false;
    long long arrived_at_ms = 0;  // equal values mean simultaneous, regardless of advance_time chunking
};

/// Single source of truth for GameEngine/GameOverObserver on what ends the game.
inline bool captured_a_king(const ArrivalEvent& event) {
    return event.captured_piece.has_value() && event.captured_piece->kind == PieceKind::King;
}

}  // namespace kfc::model
