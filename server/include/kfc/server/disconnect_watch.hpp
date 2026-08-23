#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

#include "kfc/model/piece.hpp"

namespace kfc::server {

/// Grace period a dropped player gets before forfeiting; reported from a connection thread,
/// counted down on the tick thread.
class DisconnectWatch {
public:
    explicit DisconnectWatch(int grace_ms);

    struct Tick {
        /// Set only on the tick the displayed second actually changes.
        std::optional<int> seconds_remaining;
        std::optional<kfc::model::PieceColor> expired_for;
    };

    /// Ignored if another countdown is already running; the countdown itself opens on the next advance().
    void report_disconnect(kfc::model::PieceColor color);

    /// Call once per tick with that tick's clock.
    [[nodiscard]] Tick advance(std::chrono::steady_clock::time_point now);

    [[nodiscard]] std::optional<kfc::model::PieceColor> watching() const;

    /// True from the moment a disconnect is reported, not just once advance() opens the countdown.
    [[nodiscard]] bool is_frozen() const { return frozen_.load(std::memory_order_acquire); }

    /// False if the grace already expired (caller is too late); races advance() by design.
    [[nodiscard]] bool cancel(kfc::model::PieceColor color);

    void clear();

private:
    // Mirrors pending_ || watching_; kept atomic so is_frozen() never takes the mutex.
    std::atomic<bool> frozen_{false};

    mutable std::mutex mutex_;
    int grace_ms_;

    std::optional<kfc::model::PieceColor> pending_;

    std::optional<kfc::model::PieceColor> watching_;
    std::chrono::steady_clock::time_point deadline_;
    int last_reported_second_ = -1;
};

}  // namespace kfc::server
