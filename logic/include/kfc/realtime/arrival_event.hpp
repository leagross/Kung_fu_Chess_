#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_kind.hpp"

namespace kfc::model {

/// What happened when one moving piece finished arriving at its destination.
/// moved_piece and captured_piece are full snapshots, not just ids, so
/// observers can react without re-querying Board after it has moved on.
struct ArrivalEvent {
    Piece moved_piece;
    Position source;
    Position destination;
    std::optional<Piece> captured_piece;
    /// Defaults to Move for the few test-only events that don't set it.
    MotionKind kind = MotionKind::Move;
    /// True when this arrival promoted a pawn (moved_piece.kind is already
    /// the promoted-to kind).
    bool was_promotion = false;
    /// Simulated clock time this arrival was resolved at. Two events with
    /// equal arrived_at_ms happened at the exact same instant, even if one
    /// advance_time call swept past both.
    long long arrived_at_ms = 0;
};

/// The one place "is this a king capture" is spelled out, so GameEngine and
/// GameOverObserver can't drift into disagreeing about what ends the game.
inline bool captured_a_king(const ArrivalEvent& event) {
    return event.captured_piece.has_value() && event.captured_piece->kind == PieceKind::King;
}

}  // namespace kfc::model
