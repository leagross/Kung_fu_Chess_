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

    // Sorted below so resolution order doesn't depend on the caller's step size.
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
    // A piece captured earlier in this batch must skip its own queued arrival.
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
            // Piece arrived mid-tick, so only the remainder of ms counts against cooldown.
            int time_left_in_tick_ms = ms - pending_arrival.time_into_tick_ms;
            int remaining_cooldown_ms = motion.cooldown_ms - time_left_in_tick_ms;
            if (remaining_cooldown_ms > 0) {
                cooldowns_remaining_ms_[motion.moving_piece.id] = remaining_cooldown_ms;
            }
        }
    }
    active_motions_ = std::move(still_active);
    // Drop a captured piece's own in-flight Move so it can't "resurrect" later.
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
        // Mover stays put; only its Moving flag needs undoing.
        board_.set_piece_state(motion.source, PieceState::Idle);
        Piece stayed = motion.moving_piece;
        stayed.state = PieceState::Idle;
        return ResolvedArrival{ArrivalEvent{stayed, motion.source, motion.source, std::nullopt, motion.kind},
                                /*piece_actually_arrived=*/false};
    }

    if (collision.kind == CollisionKind::EnemyCaptured || collision.kind == CollisionKind::PassedThroughAirborne) {
        board_.remove_piece(motion.destination);
    }

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
