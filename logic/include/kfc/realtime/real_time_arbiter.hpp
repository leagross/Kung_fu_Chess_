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

/// Owns in-flight motions and post-arrival cooldowns; only touches Board at arrival.
class RealTimeArbiter {
public:
    explicit RealTimeArbiter(Board& board);

    /// True if piece_id has a motion in flight or is in cooldown.
    [[nodiscard]] bool is_piece_busy(PieceId piece_id) const;

    /// Caller must already know the piece is not busy.
    void start_motion(const Motion& motion);

    /// Arrivals are resolved in arrival order, not insertion order, for deterministic collisions.
    ArrivalEvents advance_time(int ms);

    /// nullopt both when idle and when resting in cooldown.
    [[nodiscard]] std::optional<Motion> motion_for(PieceId piece_id) const;

    [[nodiscard]] int cooldown_remaining_ms(PieceId piece_id) const;

private:
    struct ResolvedArrival {
        ArrivalEvent event;
        bool piece_actually_arrived;  // false if CollisionResolver blocked it; no cooldown then
    };

    ResolvedArrival resolve_arrival(const Motion& motion);

    Board& board_;
    std::vector<Motion> active_motions_;
    std::unordered_map<PieceId, int> cooldowns_remaining_ms_;
    long long clock_ms_ = 0;
};

}  // namespace kfc::model
