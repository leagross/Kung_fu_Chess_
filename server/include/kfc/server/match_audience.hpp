#pragma once

#include <atomic>
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
class MatchAudience {
public:
    /// Seats a player in the next free colour -- White first, then Black --
    /// remembering their username and how to reach them. std::nullopt when both
    /// seats are already taken; the caller decides what to do with a third
    /// arrival (Match makes them a watcher).
    [[nodiscard]] std::optional<kfc::model::PieceColor> seat(const std::string& username, SendFn send, CloseFn close);

    /// Adds a watcher: reached by every broadcast, owns no colour, never
    /// affects the game. Unlimited -- seats are what's capped at two.
    void watch(SendFn send, CloseFn close);

    /// Swaps a seated colour's connection for a new one, for a player who
    /// dropped and came back. Their username and colour are unchanged -- it is
    /// the same player, on a different socket.
    void reseat(kfc::model::PieceColor color, SendFn send, CloseFn close);

    /// True once both seats are filled, i.e. the match can actually be played.
    /// This is what "has the match started" means -- there is no separate flag
    /// to keep in step with the seats themselves.
    ///
    /// A lock-free atomic read: Match::state() asks this for every command, and
    /// it must not have to queue behind a broadcast holding the table's mutex.
    [[nodiscard]] bool both_seats_taken() const { return seats_filled_.load(std::memory_order_acquire) == 2; }

    /// The username seated in that colour, or empty if nobody is.
    [[nodiscard]] std::string username_of(kfc::model::PieceColor color) const;

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
    // How many of the two seats are filled. Mirrors white_send_/black_send_
    // below and is written only while holding mutex_, so the two agree; it
    // exists purely so both_seats_taken() needs no lock. Never decremented --
    // a player who drops still owns their seat until the grace expires.
    std::atomic<int> seats_filled_{0};

    // One lock for the whole table: reads (broadcast, on the tick thread) and
    // writes (seating, on connection threads) genuinely interleave.
    mutable std::mutex mutex_;

    std::optional<SendFn> white_send_;
    std::optional<SendFn> black_send_;
    std::optional<CloseFn> white_close_;
    std::optional<CloseFn> black_close_;
    std::string white_username_;
    std::string black_username_;

    // Watchers, in arrival order. Never removed when one disconnects: a send or
    // close to a dead socket is a harmless no-op, and the whole table dies with
    // the room moments later anyway.
    std::vector<SendFn> spectator_sends_;
    std::vector<CloseFn> spectator_closes_;
};

}  // namespace kfc::server
