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

/// Everyone attached to one match -- the two seated players and any number of
/// watchers -- and the only way to reach them. Knows nothing about rules,
/// board, or clock.
///
/// Threading: every method is internally synchronized. Callers pass
/// already-encoded text rather than protocol messages, so encoding (and its
/// logging) stays in Match.
///
/// **No callback is ever invoked while the lock is held**: send() can block
/// on a slow socket, and a failed send calls back in as a disconnect, which
/// would deadlock on a non-recursive mutex. The roster is therefore
/// copy-on-write -- readers take a pointer under the lock and let go;
/// writers publish a new version rather than editing the one being read.
class MatchAudience {
public:
    /// White first, then Black, remembering username/rating/how to reach
    /// them. nullopt when both seats are taken; caller decides what to do
    /// with a third arrival (Match makes them a watcher).
    [[nodiscard]] std::optional<kfc::model::PieceColor> seat(const std::string& username, int rating, SendFn send,
                                                              CloseFn close);

    /// Caps one attacker from opening unbounded watch connections to a room
    /// -- a safety ceiling, not the primary defense (see RateLimiter/
    /// main.cpp's seat_limiter for that).
    static constexpr std::size_t kMaxSpectators = 5000;

    /// Returns 0 (unwatch()'s "not a watcher" sentinel) once kMaxSpectators
    /// is already attached.
    [[nodiscard]] WatcherId watch(SendFn send, CloseFn close);

    /// Unknown ids are ignored, so a double close is a no-op.
    void unwatch(WatcherId id);

    /// Swaps a seated colour's connection for a new one; same player, same
    /// colour, different socket.
    void reseat(kfc::model::PieceColor color, SendFn send, CloseFn close);

    /// Lock-free atomic: asked on every command by Match::state().
    [[nodiscard]] bool both_seats_taken() const { return seats_filled_.load(std::memory_order_acquire) == 2; }

    [[nodiscard]] std::string username_of(kfc::model::PieceColor color) const;

    /// Fixed at seat time, not re-read live; 0 if nobody is seated there yet.
    [[nodiscard]] int rating_of(kfc::model::PieceColor color) const;

    /// For tests/diagnostics -- the game never behaves differently for it.
    [[nodiscard]] std::size_t watcher_count() const;

    void broadcast(const std::string& encoded) const;

    /// No-op if that seat is empty.
    void send_to(kfc::model::PieceColor color, const std::string& encoded) const;

    /// Used once the match is decided; the room can't be reaped while anyone
    /// is still attached.
    void release_all() const;

private:
    struct Watcher {
        WatcherId id;
        SendFn send;
        CloseFn close;
    };

    // Persistent singly-linked list, newest first -- watch() conses O(1)
    // instead of deep-copying a std::vector<Watcher> on every call (which
    // made filling kMaxSpectators O(n^2); see test_match_audience.cpp).
    struct WatcherNode {
        Watcher watcher;
        std::shared_ptr<const WatcherNode> next;

        // Recursing into a long chain's compiler-generated destructor really
        // does stack-overflow (confirmed on Windows Debug at kMaxSpectators);
        // this unlinks one node at a time by hand instead.
        ~WatcherNode() {
            std::shared_ptr<const WatcherNode> current = std::move(next);
            // use_count() == 1: nothing else (e.g. a reader mid-broadcast on
            // an older Roster) still references this node, so it's ours to
            // unlink before `current` is reassigned below.
            while (current != nullptr && current.use_count() == 1) {
                std::shared_ptr<const WatcherNode> successor = std::move(const_cast<WatcherNode&>(*current).next);
                current = std::move(successor);
            }
        }
    };

    // Immutable; published by replacement so a reader mid-send always sees a
    // consistent version.
    struct Roster {
        std::optional<SendFn> white_send;
        std::optional<SendFn> black_send;
        std::optional<CloseFn> white_close;
        std::optional<CloseFn> black_close;
        std::string white_username;
        std::string black_username;
        int white_rating = 0;
        int black_rating = 0;
        std::shared_ptr<const WatcherNode> watchers;  // head of the list; nullptr means empty
        std::size_t watcher_count = 0;  // tracked alongside the list so watcher_count() need not walk it
    };

    [[nodiscard]] std::shared_ptr<const Roster> current() const;

    // Must be called with mutex_ held.
    [[nodiscard]] std::shared_ptr<Roster> editable_copy() const;

    // Mirrors the roster's two send slots so both_seats_taken() needs no
    // lock. Never decremented -- a dropped player still owns their seat
    // until the grace expires.
    std::atomic<int> seats_filled_{0};

    // Guards replacing roster_ and handing out the next watcher id. Never
    // held across a callback.
    mutable std::mutex mutex_;

    std::shared_ptr<const Roster> roster_{std::make_shared<const Roster>()};
    WatcherId next_watcher_id_ = 1;  // 0 means "not a watcher"
};

}  // namespace kfc::server
