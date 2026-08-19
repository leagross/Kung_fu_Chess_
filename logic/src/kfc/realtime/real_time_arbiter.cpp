#include "../../../include/kfc/realtime/real_time_arbiter.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace kfc::model {

RealTimeArbiter::RealTimeArbiter(Board& board) : board_(board) {}

bool RealTimeArbiter::is_piece_busy(PieceId piece_id) const {
    return active_motions_.count(piece_id) > 0 || cooldowns_remaining_ms_.count(piece_id) > 0;
}

std::optional<Motion> RealTimeArbiter::motion_for(PieceId piece_id) const {
    auto it = active_motions_.find(piece_id);
    return it != active_motions_.end() ? std::optional<Motion>(it->second) : std::nullopt;
}

int RealTimeArbiter::cooldown_remaining_ms(PieceId piece_id) const {
    auto it = cooldowns_remaining_ms_.find(piece_id);
    return it != cooldowns_remaining_ms_.end() ? it->second : 0;
}

void RealTimeArbiter::start_motion(const Motion& motion) {
    active_motions_[motion.moving_piece.id] = motion;
    board_.set_piece_state(motion.source,
                            motion.kind == MotionKind::JumpInPlace ? PieceState::Airborne : PieceState::Moving);
}

ArrivalEvents RealTimeArbiter::advance_time(int ms) {
    long long tick_start_ms = clock_ms_;
    clock_ms_ += ms;

    for (auto& [piece_id, remaining_ms] : cooldowns_remaining_ms_) {
        remaining_ms -= ms;
    }
    for (auto it = cooldowns_remaining_ms_.begin(); it != cooldowns_remaining_ms_.end();) {
        if (it->second <= 0) {
            it = cooldowns_remaining_ms_.erase(it);
        } else {
            ++it;
        }
    }

    // Motions crossing their arrival threshold this call, paired with how
    // far into the call (0..ms) each arrived -- sorted below so resolution
    // order doesn't depend on the caller's step size (e.g. advance_time(1300)
    // must behave like advance_time(1000) then advance_time(300)).
    struct PendingArrival {
        Motion motion;
        int time_into_tick_ms;
    };
    std::vector<PendingArrival> pending;
    std::unordered_map<PieceId, Motion> still_active;
    for (auto& [piece_id, motion] : active_motions_) {
        int time_until_arrival_ms = motion.duration_ms - motion.elapsed_ms;
        motion.elapsed_ms += ms;
        if (motion.elapsed_ms >= motion.duration_ms) {
            pending.push_back(PendingArrival{std::move(motion), std::clamp(time_until_arrival_ms, 0, ms)});
        } else {
            still_active.emplace(piece_id, std::move(motion));
        }
    }
    std::stable_sort(pending.begin(), pending.end(), [](const PendingArrival& a, const PendingArrival& b) {
        return a.time_into_tick_ms < b.time_into_tick_ms;
    });

    ArrivalEvents events;
    // On a head-on collision within the same call, pending is sorted
    // chronologically so whichever arrives first captures the other; the
    // loser's own motion (already queued in this batch) must be skipped so
    // it can't land using its now-stale pre-capture snapshot.
    std::unordered_set<PieceId> captured_this_batch;
    for (const PendingArrival& pending_arrival : pending) {
        const Motion& motion = pending_arrival.motion;
        if (captured_this_batch.count(motion.moving_piece.id) > 0) {
            continue;
        }
        ResolvedArrival resolved = resolve_arrival(motion);
        resolved.event.arrived_at_ms = tick_start_ms + pending_arrival.time_into_tick_ms;
        if (resolved.event.captured_piece.has_value()) {
            captured_this_batch.insert(resolved.event.captured_piece->id);
        }
        events.push_back(resolved.event);
        if (resolved.piece_actually_arrived && motion.cooldown_ms > 0) {
            // Only the remainder of this call's ms has elapsed against the
            // fresh cooldown, since the piece arrived mid-tick.
            int time_left_in_tick_ms = ms - pending_arrival.time_into_tick_ms;
            int remaining_cooldown_ms = motion.cooldown_ms - time_left_in_tick_ms;
            if (remaining_cooldown_ms > 0) {
                cooldowns_remaining_ms_[motion.moving_piece.id] = remaining_cooldown_ms;
            }
        }
    }
    active_motions_ = std::move(still_active);
    // A piece captured this tick may still have had its own Move in flight
    // (Board shows a mover at its source cell until arrival, so it stays
    // capturable mid-flight) -- drop that stale motion and cooldown now, or
    // it would "resurrect" on a later advance_time call. Always a Move,
    // never a JumpInPlace: an airborne piece is PassedThroughAirborne, never
    // EnemyCaptured (see JumpRaceTest).
    for (PieceId captured_id : captured_this_batch) {
        auto it = active_motions_.find(captured_id);
        if (it != active_motions_.end() && it->second.kind == MotionKind::Move) {
            active_motions_.erase(it);
        }
        cooldowns_remaining_ms_.erase(captured_id);
    }

    return events;
}

RealTimeArbiter::ResolvedArrival RealTimeArbiter::resolve_arrival(const Motion& motion) {
    std::optional<Piece> occupant = board_.piece_at(motion.destination);
    CollisionResult collision = CollisionResolver::resolve(motion.moving_piece, occupant);

    if (collision.kind == CollisionKind::FriendlyBlocked) {
        // Mover stays at its source cell (start_motion never relocated it);
        // only its Moving flag needs undoing.
        board_.set_piece_state(motion.source, PieceState::Idle);
        Piece stayed = motion.moving_piece;
        stayed.state = PieceState::Idle;
        return ResolvedArrival{ArrivalEvent{stayed, motion.source, motion.source, std::nullopt, motion.kind},
                                /*piece_actually_arrived=*/false};
    }

    if (collision.kind == CollisionKind::EnemyCaptured || collision.kind == CollisionKind::PassedThroughAirborne) {
        // PassedThroughAirborne also clears the cell: Board still holds the
        // airborne piece's stale record there since it never actually left.
        board_.remove_piece(motion.destination);
    }

    // Uses the mover's own snapshot, not Board's current source cell -- see
    // Motion's comment for why that matters once motions race for a cell.
    board_.remove_piece(motion.source);
    Piece arrived = motion.moving_piece;
    arrived.cell = motion.destination;
    arrived.state = PieceState::Idle;

    if (motion.kind == MotionKind::Move) {
        arrived.has_moved = true;
    }

    bool promoted = apply_pawn_promotion(arrived, board_);

    board_.add_piece(arrived);

    return ResolvedArrival{
        ArrivalEvent{arrived, motion.source, motion.destination, collision.captured_piece, motion.kind, promoted},
        /*piece_actually_arrived=*/true};
}

}  // namespace kfc::model
