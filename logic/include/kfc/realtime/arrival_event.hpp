#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"
#include "../../kfc/realtime/motion_kind.hpp"

namespace kfc::model {

/// What happened when one moving piece finished arriving at its destination.
/// moved_piece is a full post-arrival snapshot (its kind already reflects
/// promotion, if any just happened) -- carried here, not just an id, for the
/// same reason captured_piece is: callers (a move-log observer, scoring)
/// need identity and kind to react without re-querying Board, which may
/// have moved on by the time an observer actually reads this event. If a
/// piece occupied the destination at the moment arrival was resolved, it
/// was captured -- captured_piece carries its own full snapshot the same way.
struct ArrivalEvent {
    Piece moved_piece;
    Position source;
    Position destination;
    std::optional<Piece> captured_piece;
    /// Which kind of motion produced this arrival. Carried so a move log can
    /// render a JumpInPlace distinctly rather than as an ordinary chess move
    /// -- a piece that jumped and landed back on its own cell is not "Ne4".
    /// Defaults to Move for the few test-only events that don't set it.
    MotionKind kind = MotionKind::Move;
    /// True when this arrival promoted a pawn (it reached the last rank and
    /// moved_piece.kind is already the promoted-to kind). Lets a move log
    /// write "e8=Q" instead of mistaking the post-promotion queen for a
    /// queen's own move.
    bool was_promotion = false;
    /// Simulated time (RealTimeArbiter's own running total of every ms it
    /// has ever been advanced by) at which this arrival was resolved. Two
    /// events with equal arrived_at_ms genuinely happened at the same
    /// instant, even if a single coarse advance_time call swept past both
    /// -- this is what lets GameOverObserver tell "both kings captured at
    /// the exact same moment" (a draw) apart from "the second king's motion
    /// simply happened to resolve later, after the game had already ended"
    /// (not a draw; the earlier capture already decided it).
    long long arrived_at_ms = 0;
};

/// True if event represents a king being captured. The one place this
/// condition is written -- GameEngine (to reject further moves) and
/// GameOverObserver (to report who won and that the game has ended) both
/// call this instead of each spelling out captured_piece->kind == King on
/// their own, so the two can never drift into disagreeing about what "the
/// game is over" means.
inline bool captured_a_king(const ArrivalEvent& event) {
    return event.captured_piece.has_value() && event.captured_piece->kind == PieceKind::King;
}

}  // namespace kfc::model
