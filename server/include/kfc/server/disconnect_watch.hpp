#pragma once

#include <atomic>
#include <chrono>
#include <mutex>
#include <optional>

#include "kfc/model/piece.hpp"

namespace kfc::server {

/// The grace period a dropped player gets before the match is forfeited on
/// their behalf (the CTD SERVER spec's 20 seconds, counted down on the
/// opponent's screen).
///
/// Split out of Match because this is a small state machine in its own right --
/// idle, counting down, expired -- with its own clock, its own once-per-second
/// throttle, and its own thread-safety problem (a drop is reported from a
/// connection thread, the countdown runs on the tick thread, and a returning
/// player cancels it from a third). Tangled into Match it was five fields and a
/// mutex that had nothing to do with playing chess; here it is testable on its
/// own and Match simply asks it what happened this tick.
class DisconnectWatch {
public:
    /// grace_ms is how long a dropped player has to come back.
    explicit DisconnectWatch(int grace_ms);

    /// What this tick's advance() found. Both fields are usually empty: most
    /// ticks pass with nothing to report.
    struct Tick {
        /// Whole seconds still left, set only on the tick the displayed number
        /// actually changes -- so the countdown is broadcast once per second,
        /// not sixty times.
        std::optional<int> seconds_remaining;
        /// The colour whose grace just ran out. The caller forfeits the match
        /// for them; the watch has already stopped watching.
        std::optional<kfc::model::PieceColor> expired_for;
    };

    /// A player's connection dropped. Safe to call from a connection thread --
    /// it only records the fact; the countdown itself opens on the next
    /// advance(), on the tick thread. A drop reported while another countdown
    /// is already running is ignored: the room is being torn down anyway once
    /// both sides are gone.
    void report_disconnect(kfc::model::PieceColor color);

    /// Drives the countdown. Call once per tick with that tick's clock.
    [[nodiscard]] Tick advance(std::chrono::steady_clock::time_point now);

    /// Which colour is being counted down right now, if any. Safe to call from
    /// a connection thread -- this is exactly the question a joining player's
    /// "am I the one who dropped?" has to ask.
    [[nodiscard]] std::optional<kfc::model::PieceColor> watching() const;

    /// Whether a disconnect is in effect at all: a countdown already running,
    /// **or** one reported and not yet picked up by the next tick.
    ///
    /// That second case is the point. watching() only becomes true once
    /// advance() has run, so between a drop being reported on a connection
    /// thread and the tick that opens its countdown there is a window -- and a
    /// command arriving in that window would be applied against a player who
    /// has already gone. Freezing reads this, not watching().
    ///
    /// A lock-free atomic read, deliberately: it is checked for every command
    /// and every tick, so it must not queue behind the mutex that the countdown
    /// itself uses.
    [[nodiscard]] bool is_frozen() const { return frozen_.load(std::memory_order_acquire); }

    /// Stops counting down `color`, because that player came back in time.
    /// Returns false if the watch was not (or no longer) counting that colour
    /// down -- i.e. the grace already expired and the caller is too late. Safe
    /// to call from a connection thread; racing advance() is the point, and
    /// exactly one of the two wins.
    [[nodiscard]] bool cancel(kfc::model::PieceColor color);

    /// Stops counting down whatever it was watching, unconditionally -- the
    /// game ended some other way (a capture during the grace, a resign), so
    /// there is nothing left to forfeit.
    void clear();

private:
    // "A disconnect is in effect", mirroring pending_ || watching_ below.
    // Kept as an atomic so is_frozen() -- on the hot path of every command and
    // every tick -- never takes the mutex. Written only while holding it, so
    // the two can never disagree.
    std::atomic<bool> frozen_{false};

    mutable std::mutex mutex_;

    // How long a dropped player gets. Const after construction.
    int grace_ms_;

    // A drop reported but not yet turned into a countdown -- handed from a
    // connection thread to the tick thread, consumed once by advance().
    std::optional<kfc::model::PieceColor> pending_;

    // The live countdown: whose it is, when it expires, and the last whole
    // second already reported (so only changes are). All guarded by mutex_,
    // because watching()/cancel() read and clear them from other threads.
    std::optional<kfc::model::PieceColor> watching_;
    std::chrono::steady_clock::time_point deadline_;
    int last_reported_second_ = -1;
};

}  // namespace kfc::server
