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

/// Identifies one room (one Match) for the lifetime of the server process.
/// Monotonic and never reused, so a stale message for a torn-down room is
/// simply dropped rather than misrouted to whatever took its place.
using RoomId = int;

/// Owns every live Match -- one per room -- and routes each connection to
/// the right one by RoomId. Torn down once its last player disconnects.
/// Blocking calls (Welcome sends, etc.) happen with rooms_mutex_ released,
/// so a slow join never stalls routing.
class RoomManager {
public:
    /// board_factory produces a fresh starting Board per room. on_result, if
    /// set, is forwarded to every room's Match (RoomManager stays ELO-
    /// agnostic). directory/self_url, if given, register/look up rooms
    /// across workers; left null, this behaves as single-worker.
    RoomManager(std::function<kfc::model::Board()> board_factory, kfc::protocol::FileLogger& logger,
                kfc::protocol::GameplayConfig config = {}, ResultCallback on_result = {},
                int disconnect_grace_ms = kDefaultDisconnectGraceMs, IRoomDirectory* directory = nullptr,
                std::string self_url = {}, Metrics* metrics = nullptr);
    ~RoomManager();

    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    /// Which room, and the colour given. spectator marks a viewer (colour is
    /// meaningless for it: commands dropped, disconnect not a forfeit).
    struct Seat {
        RoomId room;
        kfc::model::PieceColor color;
        bool spectator = false;
        /// Which watcher this is when spectator is true; 0 for a player.
        WatcherId watcher = 0;
    };

    /// Seats this player with a waiting opponent within
    /// kMatchmakingRatingGap of their rating (closest first); otherwise
    /// opens a new room and waits. nullopt only on the defensive impossible
    /// case (chosen room rejected the join).
    [[nodiscard]] std::optional<Seat> join_any(const std::string& username, int rating, SendFn send,
                                                CloseFn close = {});

    /// Opens a room with a server-generated id and seats the creator as
    /// White, so this can't fail on a name collision.
    [[nodiscard]] std::optional<Seat> create_room(const std::string& username, int rating, SendFn send,
                                                   CloseFn close = {}, std::string* failure_reason = nullptr);

    /// Joins an existing named room: a dropped player whose grace is still
    /// running reclaims their seat; else the first opponent is seated Black;
    /// else they watch. nullopt if no such room exists or it's decided; if
    /// another worker owns it, redirect_url gets its address instead.
    [[nodiscard]] std::optional<Seat> join_room(const std::string& name, const std::string& username, int rating,
                                                 SendFn send, CloseFn close = {},
                                                 std::string* failure_reason = nullptr,
                                                 std::string* redirect_url = nullptr);

    /// No-op if the room no longer exists.
    void enqueue(RoomId room, kfc::model::PieceColor from, kfc::protocol::ClientMessage message);

    /// Ends that room's game in the opponent's favour and, once no players
    /// remain, tears it down. Takes the whole Seat (not just its parts) so a
    /// viewer's meaningless colour can never be mistaken for a player's.
    void on_disconnect(const Seat& seat);

    [[nodiscard]] std::size_t room_count() const;

    /// 0 once stop_all() has torn the scheduler down.
    [[nodiscard]] std::size_t worker_count() const;

    /// Blocks until every worker thread has exited. Idempotent. Call before
    /// shutting the socket layer down -- a frozen match's tick still sends
    /// on it every second (the destructor calls this too).
    void stop_all();

private:
    struct Room {
        // shared_ptr, not unique: a caller can keep the Match alive across
        // the gap where rooms_mutex_ is released for network I/O, even if
        // another thread reaps the room concurrently.
        std::shared_ptr<Match> match;
        // Only ever increases; a room that filled up stays unjoinable even
        // after its game ends. Availability is simply seats_taken < 2.
        int seats_taken = 0;
        // Room is reaped when this reaches zero -- see on_disconnect.
        int connected = 0;
        // Valid while seats_taken == 1; pairs a newcomer within kMatchmakingRatingGap.
        int waiting_rating = 0;
        // Empty for a Play matchmaking room.
        std::string name;
    };

    // Must be called with rooms_mutex_ held.
    Room& open_room(RoomId& id_out, std::string room_name = {});

    // Avoids characters easy to confuse when spoken/written. Retried until
    // unused. Must be called with rooms_mutex_ held.
    std::string generate_room_id();

    // Must be called with rooms_mutex_ held.
    [[nodiscard]] std::optional<RoomId> closest_waiting_room(int rating) const;

    // Safe to call unconditionally even for a room never in the index (e.g.
    // a named room). Both must be called with rooms_mutex_ held.
    void mark_waiting(RoomId id, int rating);
    void unmark_waiting(RoomId id, int rating);

    std::function<kfc::model::Board()> board_factory_;
    kfc::protocol::FileLogger& logger_;
    kfc::protocol::GameplayConfig config_;
    ResultCallback on_result_;
    int disconnect_grace_ms_;

    // Not owned; must outlive this RoomManager.
    IRoomDirectory* directory_;
    std::string self_url_;

    Metrics* metrics_;

    // Pointer, not a plain member, so stop_all() can tear it down (null
    // after makes a second call a no-op). Guarded by scheduler_mutex_, not a
    // bare null check: reset() joins every worker before storing nullptr, so
    // the mutex is what stops a connection thread from calling in mid-teardown.
    mutable std::mutex scheduler_mutex_;
    std::unique_ptr<MatchScheduler> scheduler_;

    mutable std::mutex rooms_mutex_;
    std::map<RoomId, Room> rooms_;
    // Name -> room id, for the named Room feature. Play rooms are not named.
    std::map<std::string, RoomId> named_rooms_;
    // Matchmaking rooms with exactly one seat filled, ordered by rating, so
    // join_any can find a compatible opponent in O(log w) instead of
    // scanning every room in rooms_. A named room never enters this index;
    // a matchmaking room leaves it the instant it pairs.
    std::multimap<int, RoomId> waiting_by_rating_;
    RoomId next_room_id_ = 1;
};

}  // namespace kfc::server
