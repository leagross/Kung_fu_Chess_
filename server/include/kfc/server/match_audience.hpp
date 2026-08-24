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

/// Everyone attached to one match. Internally synchronized; no callback ever runs while the lock is held.
class MatchAudience {
public:
    /// White first, then Black; nullopt when both seats are taken.
    [[nodiscard]] std::optional<kfc::model::PieceColor> seat(const std::string& username, int rating, SendFn send,
                                                              CloseFn close);

    /// Safety ceiling, not the primary defense (see RateLimiter/main.cpp's seat_limiter for that).
    static constexpr std::size_t kMaxSpectators = 5000;

    /// Returns 0 (unwatch()'s "not a watcher" sentinel) once kMaxSpectators is already attached.
    [[nodiscard]] WatcherId watch(SendFn send, CloseFn close);

    void unwatch(WatcherId id);

    /// Swaps a seated colour's connection for a new one; same player, same colour, different socket.
    void reseat(kfc::model::PieceColor color, SendFn send, CloseFn close);

    [[nodiscard]] bool both_seats_taken() const { return seats_filled_.load(std::memory_order_acquire) == 2; }

    [[nodiscard]] std::string username_of(kfc::model::PieceColor color) const;

    [[nodiscard]] int rating_of(kfc::model::PieceColor color) const;

    [[nodiscard]] std::size_t watcher_count() const;

    void broadcast(const std::string& encoded) const;

    void send_to(kfc::model::PieceColor color, const std::string& encoded) const;

    void release_all() const;

private:
    struct Watcher {
        WatcherId id;
        SendFn send;
        CloseFn close;
    };

    // Persistent singly-linked list, newest first -- watch() conses O(1) instead of copying a vector.
    struct WatcherNode {
        Watcher watcher;
        std::shared_ptr<const WatcherNode> next;

        // Recursing into the compiler-generated destructor stack-overflows on a long chain; unlink by hand.
        ~WatcherNode() {
            std::shared_ptr<const WatcherNode> current = std::move(next);
            // use_count() == 1: no other reader still references this node.
            while (current != nullptr && current.use_count() == 1) {
                std::shared_ptr<const WatcherNode> successor = std::move(const_cast<WatcherNode&>(*current).next);
                current = std::move(successor);
            }
        }
    };

    // Immutable; published by replacement so a reader mid-send always sees a consistent version.
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

    // Never decremented -- a dropped player still owns their seat until the grace expires.
    std::atomic<int> seats_filled_{0};

    mutable std::mutex mutex_;

    std::shared_ptr<const Roster> roster_{std::make_shared<const Roster>()};
    WatcherId next_watcher_id_ = 1;  // 0 means "not a watcher"
};

}  // namespace kfc::server
