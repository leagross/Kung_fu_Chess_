#pragma once

#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "kfc/model/piece.hpp"
#include "kfc/server/connection_callbacks.hpp"

namespace kfc::server {

/// Everyone attached to one match, and the only way to reach them: the two
/// seated players (White, Black) and any number of watchers.
///
/// Split out of Match because "who is connected and how do I talk to them" is a
/// separate question from "what is happening in the game" -- Match was carrying
/// ten fields and four methods of pure address-book, which had nothing to do
/// with the simulation it exists to run. Everything here is about connections;
/// nothing here knows a rule, a board, or a clock.
///
/// Threading: every method is internally synchronized. Seating and watching
/// happen on IXWebSocket connection threads while the tick thread is
/// broadcasting, so the table is guarded throughout. Callers pass already-
/// encoded text rather than protocol messages, which keeps encoding (and its
/// logging) where it belongs, in Match.
///
/// **No callback is ever invoked while the lock is held.** Sending is network
/// I/O -- a slow or half-dead socket can block inside send() for as long as its
/// TCP buffers stay full, and every seating, watching and username lookup in
/// the match would queue behind it. Worse, a send that fails calls back into the
/// server as a disconnect, which comes straight back here and deadlocks on a
/// non-recursive mutex. So the roster is copy-on-write: readers take a pointer
/// to the current version under the lock and then let go, and writers publish a
/// new version rather than editing the one being read. The lock is held for a
/// pointer copy either way -- membership changes (a few per match) pay the cost
/// of the copy, not broadcasts (several per second, per match).
class MatchAudience {
public:
    /// Seats a player in the next free colour -- White first, then Black --
    /// remembering their username and how to reach them. std::nullopt when both
    /// seats are already taken; the caller decides what to do with a third
    /// arrival (Match makes them a watcher).
    [[nodiscard]] std::optional<kfc::model::PieceColor> seat(const std::string& username, SendFn send, CloseFn close);

    /// The most watchers one match will seat. Nothing about the simulation
    /// cares how many there are -- this exists purely so one attacker cannot
    /// open unbounded watch connections to a single room and have every
    /// broadcast paid for that many times over.
    static constexpr std::size_t kMaxSpectators = 200;

    /// Adds a watcher: reached by every broadcast, owns no colour, never
    /// affects the game. Returns 0 (the same sentinel unwatch() already
    /// treats as "not a watcher") once kMaxSpectators is already attached,
    /// instead of the handle to pass to unwatch when that connection closes.
    [[nodiscard]] WatcherId watch(SendFn send, CloseFn close);

    /// Drops a watcher that disconnected. Unknown ids are ignored, so a double
    /// close (or a close arriving after the match released everyone) is a no-op
    /// rather than a surprise.
    ///
    /// Without this a watcher who left stayed in the table for the rest of the
    /// match, and every broadcast kept paying to send to a socket nobody was
    /// reading -- a room people drift in and out of would end up spending most
    /// of its sending on an audience that had gone home.
    void unwatch(WatcherId id);

    /// Swaps a seated colour's connection for a new one, for a player who
    /// dropped and came back. Their username and colour are unchanged -- it is
    /// the same player, on a different socket.
    void reseat(kfc::model::PieceColor color, SendFn send, CloseFn close);

    /// True once both seats are filled, i.e. the match can actually be played.
    /// This is what "has the match started" means -- there is no separate flag
    /// to keep in step with the seats themselves.
    ///
    /// A lock-free atomic read: Match::state() asks this for every command, and
    /// it must not have to queue behind anything at all.
    [[nodiscard]] bool both_seats_taken() const { return seats_filled_.load(std::memory_order_acquire) == 2; }

    /// The username seated in that colour, or empty if nobody is.
    [[nodiscard]] std::string username_of(kfc::model::PieceColor color) const;

    /// How many watchers are currently attached. For tests and diagnostics --
    /// the game itself never behaves differently for having an audience.
    [[nodiscard]] std::size_t watcher_count() const;

    /// Sends to both players and every watcher -- a viewer sees exactly what
    /// the players see, which is the whole point of watching.
    void broadcast(const std::string& encoded) const;

    /// Sends to one seated colour only (a move rejection is nobody else's
    /// business). A no-op if that seat is empty.
    void send_to(kfc::model::PieceColor color, const std::string& encoded) const;

    /// Closes every connection -- players and watchers alike. Used once the
    /// match is decided: nothing is left to see, and the room cannot be reaped
    /// while anyone is still attached to it.
    void release_all() const;

private:
    // One watcher: how to reach it, how to let it go, and which one it is.
    struct Watcher {
        WatcherId id;
        SendFn send;
        CloseFn close;
    };

    // Everyone attached, as one immutable value. Published by replacement, so
    // any thread already reading a version keeps reading a consistent one --
    // the alternative, mutating in place, is what forces a reader to hold the
    // lock for the whole of its send.
    struct Roster {
        std::optional<SendFn> white_send;
        std::optional<SendFn> black_send;
        std::optional<CloseFn> white_close;
        std::optional<CloseFn> black_close;
        std::string white_username;
        std::string black_username;
        std::vector<Watcher> watchers;  // in arrival order
    };

    // The version to read from. One lock acquisition and one refcount bump --
    // no allocation, no I/O, nothing that can block for long.
    [[nodiscard]] std::shared_ptr<const Roster> current() const;

    // A copy of the current roster for a writer to modify and then publish into
    // roster_. Must be called with mutex_ held.
    [[nodiscard]] std::shared_ptr<Roster> editable_copy() const;

    // How many of the two seats are filled. Mirrors the roster's two send
    // slots and is written only while holding mutex_, so the two agree; it
    // exists purely so both_seats_taken() needs no lock. Never decremented --
    // a player who drops still owns their seat until the grace expires.
    std::atomic<int> seats_filled_{0};

    // Guards the two writes below: replacing roster_, and handing out the next
    // watcher id. Held only for those, never across a callback.
    mutable std::mutex mutex_;

    std::shared_ptr<const Roster> roster_{std::make_shared<const Roster>()};
    WatcherId next_watcher_id_ = 1;  // 0 means "not a watcher"
};

}  // namespace kfc::server
