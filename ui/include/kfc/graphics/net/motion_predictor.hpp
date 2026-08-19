#pragma once

#include <chrono>
#include <optional>
#include <unordered_map>

#include "kfc/realtime/motion.hpp"

namespace kfc::graphics::net {

/// Local, purely-visual guess at where an in-flight piece is, ticked forward
/// between BoardUpdates so PieceAnimator has something to draw before the
/// server confirms the real outcome. A prediction is discarded the moment
/// its piece appears in an arrival, matched or not -- this class never
/// decides anything, it only smooths what's already been decided elsewhere.
class MotionPredictor {
public:
    /// Registers a freshly-started motion. started_at is when it actually
    /// began (already adjusted for the server's own elapsed_ms), so tick()
    /// stays aligned with the instant the server -- and the other client --
    /// started counting from.
    void start(const kfc::model::Motion& motion, std::chrono::steady_clock::time_point started_at);

    /// Drops any prediction for piece_id. Safe to call for an untracked id.
    void discard(kfc::model::PieceId piece_id);

    /// Advances every tracked motion's elapsed_ms to now, clamped to its own
    /// duration_ms so a slow network never lets a prediction visually
    /// overshoot before the real arrival confirms it.
    void tick(std::chrono::steady_clock::time_point now);

    [[nodiscard]] std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const;
    [[nodiscard]] bool is_tracked(kfc::model::PieceId piece_id) const;

private:
    struct Entry {
        kfc::model::Motion motion;
        std::chrono::steady_clock::time_point started_at;
    };
    std::unordered_map<kfc::model::PieceId, Entry> predictions_;
};

}  // namespace kfc::graphics::net
