#include "../../../include/kfc/realtime/real_time_arbiter.hpp"

#include <algorithm>
#include <unordered_set>
#include <utility>

namespace kfc::model {

RealTimeArbiter::RealTimeArbiter(Board& board) : board_(board) {}

bool RealTimeArbiter::is_piece_busy(PieceId piece_id) const {
    for (const Motion& motion : active_motions_) {
        if (motion.moving_piece.id == piece_id) {
            return true;
        }
    }
    return cooldowns_remaining_ms_.count(piece_id) > 0;
}

std::optional<Motion> RealTimeArbiter::motion_for(PieceId piece_id) const {
    for (const Motion& motion : active_motions_) {
        if (motion.moving_piece.id == piece_id) {
            return motion;
        }
    }
    return std::nullopt;
}

int RealTimeArbiter::cooldown_remaining_ms(PieceId piece_id) const {
    auto it = cooldowns_remaining_ms_.find(piece_id);
    return it != cooldowns_remaining_ms_.end() ? it->second : 0;
}

void RealTimeArbiter::start_motion(const Motion& motion) {
    active_motions_.push_back(motion);
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

    // Motions that cross their arrival threshold during this call, paired
    // with how far into this call (0..ms) each one actually arrived. A
    // single coarse-grained advance_time call can sweep past several
    // arrivals at once; resolving them in active_motions_'s insertion order
    // (as opposed to the order they actually arrived within this tick)
    // would make advance_time(1300) behave differently from
    // advance_time(1000) followed by advance_time(300) -- the caller's
    // choice of step size must never change the outcome.
    struct PendingArrival {
        Motion motion;
        int time_into_tick_ms;
    };
    std::vector<PendingArrival> pending;
    std::vector<Motion> still_active;
    for (Motion motion : active_motions_) {
        int time_until_arrival_ms = motion.duration_ms - motion.elapsed_ms;
        motion.elapsed_ms += ms;
        if (motion.elapsed_ms >= motion.duration_ms) {
            pending.push_back(PendingArrival{motion, std::clamp(time_until_arrival_ms, 0, ms)});
        } else {
            still_active.push_back(motion);
        }
    }
    std::stable_sort(pending.begin(), pending.end(), [](const PendingArrival& a, const PendingArrival& b) {
        return a.time_into_tick_ms < b.time_into_tick_ms;
    });

    ArrivalEvents events;
    // Two motions can reach arrival in the same advance_time call and
    // target each other's cells (a head-on collision). Now that pending is
    // sorted chronologically, whichever actually arrives first captures the
    // other; on an exact tie (both time_into_tick_ms equal), the
    // stable_sort above preserves start order as the deterministic
    // tiebreak. The loser's own motion must not still land somewhere
    // afterward using its now-stale snapshot -- that would resurrect a
    // piece that was just captured. Tracking ids captured earlier in this
    // same batch is how a later iteration knows its mover no longer exists.
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
            // The piece arrived time_into_tick_ms into this call, so only
            // the remainder of this call's ms has actually elapsed against
            // its fresh cooldown so far -- crediting the whole cooldown
            // unconditionally would (again) make coarse and chunked
            // advance_time calls disagree.
            int time_left_in_tick_ms = ms - pending_arrival.time_into_tick_ms;
            int remaining_cooldown_ms = motion.cooldown_ms - time_left_in_tick_ms;
            if (remaining_cooldown_ms > 0) {
                cooldowns_remaining_ms_[motion.moving_piece.id] = remaining_cooldown_ms;
            }
        }
    }
    active_motions_ = std::move(still_active);
    // A piece captured this tick may still have had its own Move in flight
    // (Board only shows a moving piece at its source cell until arrival, so
    // it stays capturable there while mid-flight) -- that stale Move, and
    // any leftover cooldown entry, must be dropped now. Otherwise it would
    // "resurrect" on a later advance_time call, landing wherever its
    // pre-capture snapshot was heading -- possibly even capturing the very
    // piece that captured it. A JumpInPlace never needs this: while airborne
    // a piece is CollisionResolver::PassedThroughAirborne, never
    // EnemyCaptured, so a captured piece's own in-flight motion is always a
    // Move -- see JumpRaceTest for the airborne case this leaves alone.
    for (PieceId captured_id : captured_this_batch) {
        std::erase_if(active_motions_, [captured_id](const Motion& m) {
            return m.moving_piece.id == captured_id && m.kind == MotionKind::Move;
        });
        cooldowns_remaining_ms_.erase(captured_id);
    }

    return events;
}

RealTimeArbiter::ResolvedArrival RealTimeArbiter::resolve_arrival(const Motion& motion) {
    std::optional<Piece> occupant = board_.piece_at(motion.destination);
    CollisionResult collision = CollisionResolver::resolve(motion.moving_piece, occupant);

    if (collision.kind == CollisionKind::FriendlyBlocked) {
        // The mover never displaces an ally -- it stays at its own source
        // cell, exactly as if the motion had not happened. Board already
        // shows it there (start_motion never actually relocated it); only
        // its Moving flag needs undoing.
        board_.set_piece_state(motion.source, PieceState::Idle);
        Piece stayed = motion.moving_piece;
        stayed.state = PieceState::Idle;
        return ResolvedArrival{ArrivalEvent{stayed, motion.source, motion.source, std::nullopt, motion.kind},
                                /*piece_actually_arrived=*/false};
    }

    if (collision.kind == CollisionKind::EnemyCaptured || collision.kind == CollisionKind::PassedThroughAirborne) {
        // EnemyCaptured: the occupant is removed as a capture (already
        // snapshotted into collision.captured_piece). PassedThroughAirborne:
        // no capture happened, but Board still holds the airborne piece's
        // stale record at this cell (it never actually left), so it must be
        // cleared the same way to make room for the mover -- the airborne
        // piece is untouched otherwise and will find whoever is here now
        // when its own jump resolves.
        board_.remove_piece(motion.destination);
    }

    // Placing the mover uses its own snapshot, not whatever Board's source
    // cell currently holds -- see the comment on Motion for why that
    // distinction matters once motions can race for the same cell.
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
