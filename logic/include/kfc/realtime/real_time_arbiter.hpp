#pragma once

#include <optional>
#include <unordered_map>
#include <vector>

#include "../../kfc/model/board.hpp"
#include "../../kfc/realtime/arrival_event.hpp"
#include "../../kfc/realtime/collision_resolver.hpp"
#include "../../kfc/realtime/motion.hpp"
#include "../../kfc/realtime/pawn_promotion.hpp"

namespace kfc::model {

using ArrivalEvents = std::vector<ArrivalEvent>;

/// Owns every in-flight Motion and every piece's post-arrival cooldown, and
/// advances simulated time. Isolated from Controller and the renderer; only
/// ever touches Board's logical occupancy at the exact moment a motion
/// arrives. Receives only fully-formed Motion objects (see MotionFactory)
/// that were already validated -- it trusts the caller the same way Board
/// does, and never computes duration or cooldown itself.
class RealTimeArbiter {
public:
    /// board must outlive this RealTimeArbiter.
    explicit RealTimeArbiter(Board& board);

    /// True if piece_id currently has a motion in flight or is resting in
    /// cooldown -- either way, it cannot be commanded again yet. This
    /// replaces a single global "one motion at a time" rule: different
    /// pieces are free to move or jump independently and simultaneously.
    [[nodiscard]] bool is_piece_busy(PieceId piece_id) const;

    /// Begins tracking motion, and marks the piece at motion.source as
    /// Piece::state == Moving. Not validated here -- the caller (GameEngine,
    /// via MotionFactory) must already know piece_id is not busy.
    void start_motion(const Motion& motion);

    /// Advances simulated time by ms for every active motion and every
    /// running cooldown. Any motion whose elapsed time reaches its duration
    /// arrives: resolved in start order, each arrival atomic (capture
    /// whatever now occupies destination, then place the piece, then start
    /// that piece's cooldown countdown from the motion's cooldown_ms). If
    /// two motions arrive in the same call and collide head-on, whichever
    /// started first captures the other; the captured piece's own motion
    /// (already queued in this same batch) is dropped rather than resolved,
    /// so it cannot land anywhere using its now-stale pre-capture snapshot.
    ArrivalEvents advance_time(int ms);

    /// The in-flight Motion for piece_id, if any -- for a renderer that
    /// needs to interpolate a piece's on-screen position mid-move, or tell
    /// a Move in flight apart from a JumpInPlace one. std::nullopt both when
    /// the piece isn't busy at all, and when it's busy but only in
    /// post-arrival cooldown (advance_time already resolved and forgot that
    /// Motion by then; is_piece_busy is still true, this is not).
    [[nodiscard]] std::optional<Motion> motion_for(PieceId piece_id) const;

    /// How many more ms piece_id will stay in its post-arrival rest
    /// cooldown, or 0 if it isn't resting (whether because it's idle, has
    /// no cooldown at all, or is mid-flight -- motion_for is what tells
    /// those apart). This is the one place cooldown time actually lives;
    /// a renderer that wants its rest-animation timing to genuinely match
    /// how long the piece is unmovable for should read it from here rather
    /// than keeping its own separately-configured duration.
    [[nodiscard]] int cooldown_remaining_ms(PieceId piece_id) const;

private:
    /// The outcome of resolving one motion's arrival, plus whether the
    /// mover actually reached its destination -- false when CollisionResolver
    /// found a friendly piece there and blocked it, in which case no
    /// post-arrival cooldown should start (the move never really happened).
    struct ResolvedArrival {
        ArrivalEvent event;
        bool piece_actually_arrived;
    };

    /// Resolves one arrival atomically, delegating to CollisionResolver to
    /// decide what the mover finds at its destination. On an enemy capture,
    /// the occupant's snapshot (exposed via ArrivalEvent::captured_piece) is
    /// marked Piece::state == Captured before Board forgets it entirely. On
    /// a friendly block, the mover stays at its own source cell instead of
    /// displacing its ally. On PassedThroughAirborne, no one is captured,
    /// but Board's stale record of the airborne occupant is cleared to make
    /// room for the mover -- see the .cpp for why. Otherwise the mover is placed at destination,
    /// marked Piece::state == Idle again, and -- for an ordinary Move, never
    /// for a JumpInPlace -- Piece::has_moved becomes true. Finally,
    /// promotion: a Pawn that lands on row 0 (White) or the board's last row
    /// (Black) becomes a Queen before being placed. Promotion happens here,
    /// not in PawnRule, because it is a consequence of arrival, not of
    /// legality.
    ResolvedArrival resolve_arrival(const Motion& motion);

    Board& board_;
    std::vector<Motion> active_motions_;
    std::unordered_map<PieceId, int> cooldowns_remaining_ms_;
    /// Running total of every ms advance_time has ever been called with --
    /// gives every ArrivalEvent an absolute timestamp regardless of how
    /// coarsely or finely the caller chunks its wait() calls.
    long long clock_ms_ = 0;
};

}  // namespace kfc::model
