#pragma once

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "kfc/model/board.hpp"
#include "kfc/model/piece.hpp"
#include "kfc/protocol/gameplay_config.hpp"
#include "kfc/protocol/messages.hpp"
#include "kfc/server/match.hpp"
#include "kfc/server/match_scheduler.hpp"
#include "kfc/server/metrics.hpp"
#include "kfc/server/room_directory.hpp"

namespace kfc::protocol {
class FileLogger;
}

namespace kfc::server {

/// Monotonic and never reused, so a stale message for a torn-down room is dropped, not misrouted.
using RoomId = int;

/// Owns every live Match -- one per room -- routing each connection by RoomId.
class RoomManager {
public:
    /// directory/self_url, if given, register/look up rooms across workers; left null, single-worker.
    RoomManager(std::function<kfc::model::Board()> board_factory, kfc::protocol::FileLogger& logger,
                kfc::protocol::GameplayConfig config = {}, ResultCallback on_result = {},
                int disconnect_grace_ms = kDefaultDisconnectGraceMs, IRoomDirectory* directory = nullptr,
                std::string self_url = {}, Metrics* metrics = nullptr);
    ~RoomManager();

    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    /// spectator marks a viewer: colour is meaningless (commands dropped, disconnect not a forfeit).
    struct Seat {
        RoomId room;
        kfc::model::PieceColor color;
        bool spectator = false;
        WatcherId watcher = 0;
    };

    /// Seats with a waiting opponent within kMatchmakingRatingGap (closest first); else opens a new room.
    [[nodiscard]] std::optional<Seat> join_any(const std::string& username, int rating, SendFn send,
                                                CloseFn close = {});

    /// Server-generated id, creator seated as White; can't fail on a name collision.
    [[nodiscard]] std::optional<Seat> create_room(const std::string& username, int rating, SendFn send,
                                                   CloseFn close = {}, std::string* failure_reason = nullptr);

    /// A dropped player whose grace is still running reclaims their seat; else Black; else watch.
    [[nodiscard]] std::optional<Seat> join_room(const std::string& name, const std::string& username, int rating,
                                                 SendFn send, CloseFn close = {},
                                                 std::string* failure_reason = nullptr,
                                                 std::string* redirect_url = nullptr);

    void enqueue(RoomId room, kfc::model::PieceColor from, kfc::protocol::ClientMessage message);

    /// Takes the whole Seat, not just its parts, so a viewer's meaningless colour can't be mistaken for a player's.
    void on_disconnect(const Seat& seat);

    [[nodiscard]] std::size_t room_count() const;

    [[nodiscard]] std::size_t worker_count() const;

    /// Idempotent; call before shutting the socket layer down (the destructor also calls this).
    void stop_all();

private:
    struct Room {
        // shared_ptr so a caller can keep the Match alive while rooms_mutex_ is released for I/O.
        std::shared_ptr<Match> match;
        int seats_taken = 0;
        int connected = 0;
        int waiting_rating = 0;
        std::string name;
    };

    // Must be called with rooms_mutex_ held.
    Room& open_room(RoomId& id_out, std::string room_name = {});

    // Avoids characters easy to confuse when spoken/written. Must be called with rooms_mutex_ held.
    std::string generate_room_id();

    [[nodiscard]] std::optional<RoomId> closest_waiting_room(int rating) const;

    // Both must be called with rooms_mutex_ held.
    void mark_waiting(RoomId id, int rating);
    void unmark_waiting(RoomId id, int rating);

    std::function<kfc::model::Board()> board_factory_;
    kfc::protocol::FileLogger& logger_;
    kfc::protocol::GameplayConfig config_;
    ResultCallback on_result_;
    int disconnect_grace_ms_;

    IRoomDirectory* directory_;
    std::string self_url_;

    Metrics* metrics_;

    // scheduler_mutex_ stops a connection thread from calling in mid-teardown, not just a null check.
    mutable std::mutex scheduler_mutex_;
    std::unique_ptr<MatchScheduler> scheduler_;

    mutable std::mutex rooms_mutex_;
    std::map<RoomId, Room> rooms_;
    std::map<std::string, RoomId> named_rooms_;
    // One seat filled, ordered by rating, so join_any finds a match in O(log w) not O(n).
    std::multimap<int, RoomId> waiting_by_rating_;
    RoomId next_room_id_ = 1;
};

}  // namespace kfc::server
