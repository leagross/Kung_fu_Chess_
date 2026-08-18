#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

#include "kfc/model/piece.hpp"

namespace kfc::server {

/// Grace period a dropped player gets before the match is forfeited for them.
///
/// A small state machine (idle, counting down, expired) with its own
/// thread-safety: a drop is reported from a connection thread, the countdown
/// runs on the tick thread, and a returning player cancels it from a third.
class DisconnectWatch {
public:
    /// grace_ms is how long a dropped player has to come back.
    explicit DisconnectWatch(int grace_ms);

    /// Both fields are usually empty: most ticks have nothing to report.
    struct Tick {
        /// Set only on the tick the displayed second actually changes.
        std::optional<int> seconds_remaining;
        /// Colour whose grace just ran out; the watch has already stopped.
        std::optional<kfc::model::PieceColor> expired_for;
    };

    /// Safe to call from a connection thread; only records the fact, the
    /// countdown opens on the next advance() on the tick thread. Ignored if
    /// another countdown is already running.
    void report_disconnect(kfc::model::PieceColor color);

    /// Drives the countdown. Call once per tick with that tick's clock.
    [[nodiscard]] Tick advance(std::chrono::steady_clock::time_point now);

    /// Safe to call from a connection thread.
    [[nodiscard]] std::optional<kfc::model::PieceColor> watching() const;

    /// True from the moment a disconnect is reported, not just once
    /// advance() has opened its countdown -- otherwise a command arriving in
    /// that window would be applied against a player who already left.
    /// Lock-free atomic: checked on every command and every tick.
    [[nodiscard]] bool is_frozen() const { return frozen_.load(std::memory_order_acquire); }

    /// Returns false if the grace already expired (caller is too late). Safe
    /// to call from a connection thread; races advance() by design.
    [[nodiscard]] bool cancel(kfc::model::PieceColor color);

    /// Stops counting down unconditionally -- the game ended some other way.
    void clear();

private:
    // Mirrors pending_ || watching_; kept atomic so is_frozen() never takes
    // the mutex. Written only while holding it.
    std::atomic<bool> frozen_{false};

    mutable std::mutex mutex_;
    int grace_ms_;

    // A drop reported but not yet turned into a countdown.
    std::optional<kfc::model::PieceColor> pending_;

    // Guarded by mutex_; watching()/cancel() read and clear from other threads.
    std::optional<kfc::model::PieceColor> watching_;
    std::chrono::steady_clock::time_point deadline_;
    int last_reported_second_ = -1;
};

}  // namespace kfc::server
