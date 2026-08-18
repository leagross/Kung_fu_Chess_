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
/// advances simulated time. Only ever touches Board's occupancy at the exact
/// moment a motion arrives. Receives already-validated Motion objects (see
/// MotionFactory) and never computes duration or cooldown itself.
class RealTimeArbiter {
public:
    /// board must outlive this RealTimeArbiter.
    explicit RealTimeArbiter(Board& board);

    /// True if piece_id has a motion in flight or is resting in cooldown --
    /// either way it cannot be commanded again yet. Pieces move/jump
    /// independently, not under a single global "one motion at a time" rule.
    [[nodiscard]] bool is_piece_busy(PieceId piece_id) const;

    /// Begins tracking motion and marks the piece at motion.source as
    /// Moving. Not validated here -- caller must already know it's not busy.
    void start_motion(const Motion& motion);

    /// Advances simulated time by ms for every active motion and cooldown.
    /// Any motion whose elapsed time reaches its duration arrives, resolved
    /// in arrival order (not insertion order) so head-on collisions resolve
    /// deterministically regardless of how the caller chunks its calls.
    ArrivalEvents advance_time(int ms);

    /// The in-flight Motion for piece_id, if any. std::nullopt both when
    /// idle and when only resting in post-arrival cooldown (is_piece_busy
    /// is still true then; this is not).
    [[nodiscard]] std::optional<Motion> motion_for(PieceId piece_id) const;

    /// How many more ms piece_id stays in post-arrival cooldown, or 0.
    [[nodiscard]] int cooldown_remaining_ms(PieceId piece_id) const;

private:
    /// piece_actually_arrived is false when CollisionResolver found a
    /// friendly piece blocking it -- no cooldown should start then.
    struct ResolvedArrival {
        ArrivalEvent event;
        bool piece_actually_arrived;
    };

    /// Resolves one arrival, delegating to CollisionResolver for what the
    /// mover finds at its destination, then applies pawn promotion.
    ResolvedArrival resolve_arrival(const Motion& motion);

    Board& board_;
    std::vector<Motion> active_motions_;
    std::unordered_map<PieceId, int> cooldowns_remaining_ms_;
    /// Running total of every ms advance_time has been called with, so every
    /// ArrivalEvent gets an absolute timestamp regardless of call chunking.
    long long clock_ms_ = 0;
};

}  // namespace kfc::model
