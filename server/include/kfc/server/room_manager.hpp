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
#include "kfc/server/room_directory.hpp"

namespace kfc::protocol {
class FileLogger;
}

namespace kfc::server {

/// Identifies one room (one Match) for the lifetime of the server process.
/// Monotonic and never reused, so a stale message naming a torn-down room is
/// simply dropped rather than misrouted to whatever took its place.
using RoomId = int;

/// Owns every live Match -- one per room -- and routes each connection to the
/// right one. This is what replaces Phase 1's single global Match: the server
/// can now host any number of independent two-player games at once.
///
/// A connection is placed in a room the moment it logs in (join_any: fill a
/// room that still has an open seat, else open a fresh one), and every later
/// message from that connection is routed back to the same room by id. A room
/// is torn down -- unregistered from the MatchScheduler that was ticking it
/// -- once the last player in it has disconnected, so finished games don't
/// leak memory.
///
/// Explicit "create/join this specific room" and a lobby to drive it are a
/// later stage; this layer is deliberately just auto-matchmaking, so the
/// existing client keeps working unchanged (it still only sends Login) while
/// the server gains true multi-game support underneath it.
///
/// Board/config: each room gets its own fresh Board from board_factory and its
/// own copy of the shared GameplayConfig, so no two games ever share mutable
/// state.
///
/// Threading: IXWebSocket calls these from connection threads. Only the room
/// table and its per-room seat/connection counters are guarded here; Match's
/// own methods are already internally synchronized, and the ones that can
/// block or do network I/O (join's Welcome send, on_disconnect's hand-off)
/// are called with rooms_mutex_ released, so a slow join never stalls move
/// routing on the hot path. Every room's Match is ticked by a MatchScheduler
/// this RoomManager owns -- see match_scheduler.hpp for why that replaced a
/// thread per Match.
class RoomManager {
public:
    /// board_factory produces a fresh starting Board per room (called once
    /// each time a room is opened). logger must outlive this RoomManager.
    /// config is the shared gameplay timing/values, copied into every room.
    /// on_result, if set, is handed to every room's Match so a finished game
    /// reports its outcome for rating (see Match::ResultCallback); RoomManager
    /// just forwards it, staying unaware of ELO or the account store itself.
    /// disconnect_grace_ms is passed to every room's Match -- how long a dropped
    /// player has to return before the match is forfeited. Defaults to the
    /// spec's 20 seconds; tests pass a short value so a countdown they need to
    /// see expire does not cost twenty real seconds of suite time.
    /// directory, if given, is where this RoomManager registers rooms it
    /// creates and looks up names it doesn't recognize -- see IRoomDirectory.
    /// Left null (the default), a RoomManager behaves exactly as it always
    /// has: single-worker, no redirect ever considered. self_url is this
    /// worker's own client-facing address, registered alongside every room it
    /// creates; required (by the caller, not enforced here) whenever
    /// directory is non-null.
    RoomManager(std::function<kfc::model::Board()> board_factory, kfc::protocol::FileLogger& logger,
                kfc::protocol::GameplayConfig config = {}, ResultCallback on_result = {},
                int disconnect_grace_ms = kDefaultDisconnectGraceMs, IRoomDirectory* directory = nullptr,
                std::string self_url = {});
    ~RoomManager();

    RoomManager(const RoomManager&) = delete;
    RoomManager& operator=(const RoomManager&) = delete;

    /// Where a connection ended up: which room, and the colour it was given.
    /// spectator marks a viewer (a third or later joiner of a named room): it is
    /// in the room and receives its broadcasts, but colour is meaningless for it
    /// -- its commands are dropped and its disconnect is not a forfeit, so
    /// callers must branch on this rather than on colour.
    struct Seat {
        RoomId room;
        kfc::model::PieceColor color;
        bool spectator = false;
        /// Which watcher this is, when spectator is true; 0 for a player. The
        /// connection has to hand this back on disconnect -- it is the only
        /// thing that says which of the room's viewers just left.
        WatcherId watcher = 0;
    };

    /// Play-button matchmaking join. Seats this player with a waiting opponent
    /// whose rating is within kMatchmakingRatingGap of theirs (closest match
    /// first); if none is waiting, opens a new room and waits there until a
    /// rating-compatible opponent arrives -- so two players more than the gap
    /// apart never auto-pair (that's what named Rooms are for). rating is the
    /// player's current ELO (from authentication). Sends the Welcome and
    /// returns the resulting seat. std::nullopt is only the defensive
    /// impossible case (the chosen room rejected the join). close is how the
    /// room releases this player once its game is decided (see
    /// Match::release_players) -- forwarded untouched, like send.
    [[nodiscard]] std::optional<Seat> join_any(const std::string& username, int rating, SendFn send,
                                                CloseFn close = {});

    /// Opens a new room with a freshly *generated* id and seats the creator as
    /// White (the Room dialog's Create button). The id is the server's to mint,
    /// not the client's to choose -- so this cannot fail on a name collision,
    /// and the creator learns their id from the Welcome (Welcome::room) to read
    /// out to whoever they arranged to play. failure_reason, if given, is filled
    /// with a kfc::protocol::join_reasons string on the (currently unreachable)
    /// failure paths, so the caller can always tell the client something.
    [[nodiscard]] std::optional<Seat> create_room(const std::string& username, SendFn send, CloseFn close = {},
                                                   std::string* failure_reason = nullptr);

    /// Joins an existing named room (the Room dialog's Join button):
    ///  - if a player of this room dropped and their grace is still running and
    ///    the username matches theirs, they get their own seat and colour back
    ///    and the countdown is cancelled (see Match::reclaimable_seat_for) --
    ///    which is what tells a returning player apart from a stranger;
    ///  - else the first opponent to arrive is seated Black;
    ///  - else they watch (Seat::spectator) -- the spec's "the following people
    ///    who join are viewers".
    ///
    /// std::nullopt if no room by that name exists, or if the room's game is
    /// already decided; failure_reason (if given) receives the matching
    /// kfc::protocol::join_reasons string. If name isn't a room this worker
    /// knows about *and* directory_ says a different worker owns it,
    /// redirect_url (if given) receives that worker's address instead of
    /// failure_reason being touched at all -- the caller's cue to send a
    /// JoinRedirect rather than a JoinFailed.
    [[nodiscard]] std::optional<Seat> join_room(const std::string& name, const std::string& username, SendFn send,
                                                 CloseFn close = {}, std::string* failure_reason = nullptr,
                                                 std::string* redirect_url = nullptr);

    /// Routes one decoded client message to the given room's Match. A no-op if
    /// the room no longer exists (already torn down) -- a late frame from a
    /// player whose game already ended is simply dropped.
    void enqueue(RoomId room, kfc::model::PieceColor from, kfc::protocol::ClientMessage message);

    /// A connection dropped: ends that room's game in the opponent's favour
    /// (exactly as a resign would -- see Match::on_disconnect) and, once no
    /// players remain in the room, unregisters it from the scheduler and
    /// removes it.
    ///
    /// Takes the whole Seat the connection was given, rather than its parts,
    /// because they must agree: a viewer's colour is meaningless and passing it
    /// as a player's would forfeit an innocent game. For a viewer this forfeits
    /// nothing -- it unregisters that one watcher and drops the room's
    /// live-connection count, and a room whose viewers all left mid-game
    /// carries on exactly as before.
    void on_disconnect(const Seat& seat);

    /// Number of rooms currently alive -- for tests and diagnostics.
    [[nodiscard]] std::size_t room_count() const;

    /// Tears down the scheduler backing every room and does not return until
    /// every one of its worker threads has actually exited -- so no tick can
    /// still be in progress once this returns. Idempotent -- a second call
    /// finds the scheduler already gone and does nothing.
    ///
    /// **Call this before shutting the socket layer down**, not after. A
    /// frozen match's tick broadcasts an OpponentDisconnected every second,
    /// and broadcasting means calling send() on the transport's sockets.
    /// Tearing the transport down first leaves those two racing: a worker
    /// thread can still be inside a send on a socket that is being closed
    /// underneath it. Found by the end-to-end tests, which is the one place a
    /// real socket is involved.
    ///
    /// The destructor calls this too, for the case where nobody remembered.
    void stop_all();

private:
    struct Room {
        // Shared, not unique, so a caller can keep the Match alive across the
        // gap where rooms_mutex_ is released.
        //
        // Every method here looks a room up under the lock and then uses its
        // Match outside it -- joining sends a Welcome, which is network I/O and
        // must not block the routing hot path. But between those two moments
        // another thread's disconnect can take the room's last connection,
        // reap it, and destroy the Match: a plain pointer would be dangling by
        // the time it was used. Holding a shared_ptr means the object survives
        // until the last user is finished with it, whoever reaps it first.
        std::shared_ptr<Match> match;
        // Seats ever handed out (0..2). Only ever increases -- a seat is never
        // freed, so a room that filled up is never joinable again even after a
        // disconnect ends its game (which is what we want: that game is over).
        // Availability is simply seats_taken < 2.
        int seats_taken = 0;
        // Connections currently live in this room. The room is reaped when
        // this reaches zero -- see on_disconnect.
        int connected = 0;
        // Rating of the player waiting alone in this room (valid while
        // seats_taken == 1). Used to pair a newcomer only with an opponent
        // within kMatchmakingRatingGap.
        int waiting_rating = 0;
        // The room's name if it's a named (Room-feature) room, empty for a Play
        // matchmaking room. Kept so reaping the room can also drop its
        // named_rooms_ entry, freeing the name for reuse.
        std::string name;
    };

    // Creates a fresh room (a Match registered with scheduler_), inserts it,
    // and returns it with its new id. room_name is the displayable id a named
    // room carries (empty for a matchmaking room); the Match keeps it so its
    // Welcomes can report it. Must be called with rooms_mutex_ held.
    Room& open_room(RoomId& id_out, std::string room_name = {});

    // A short, unique, say-it-out-loud room id -- the creator has to read it to
    // whoever they arranged to play, so it avoids characters that are easy to
    // confuse when spoken or written down. Retried until unused. Must be called
    // with rooms_mutex_ held (it reads named_rooms_).
    std::string generate_room_id();

    // The closest entry in waiting_by_rating_ to rating, if any is within
    // kMatchmakingRatingGap -- see join_any and the class comment on
    // waiting_by_rating_. Must be called with rooms_mutex_ held.
    [[nodiscard]] std::optional<RoomId> closest_waiting_room(int rating) const;

    // Records/forgets that a matchmaking room is the one currently waiting
    // for an opponent, at this rating. A no-op if id isn't actually in the
    // index (unmark_waiting only), which is what makes it safe to call from
    // every reaping path unconditionally rather than needing each call site
    // to first work out whether this particular room was ever in it (a named
    // room, for instance, never is). Both must be called with rooms_mutex_
    // held.
    void mark_waiting(RoomId id, int rating);
    void unmark_waiting(RoomId id, int rating);

    std::function<kfc::model::Board()> board_factory_;
    kfc::protocol::FileLogger& logger_;
    kfc::protocol::GameplayConfig config_;
    ResultCallback on_result_;
    int disconnect_grace_ms_;

    // Null in the (default) single-worker case -- see the constructor's doc
    // comment. Not owned: main() owns the concrete RedisRoomDirectory and
    // must outlive this RoomManager, same lifetime contract as logger_.
    IRoomDirectory* directory_;
    std::string self_url_;

    // Ticks every room's Match -- see match_scheduler.hpp. A pointer (not a
    // plain member) so stop_all() can actually tear it down and join its
    // worker threads instead of merely emptying it; scheduler_ is null after
    // that, which is what makes a second stop_all() call a no-op.
    //
    // Guarded by scheduler_mutex_, not just checked for null: a bare null
    // check (what this used to be) is not enough, because "the pointer is
    // non-null" and "the MatchScheduler it points to is not currently being
    // destroyed" are different facts. unique_ptr::reset() runs the pointee's
    // destructor -- which joins every worker thread, taking real time --
    // *before* it stores nullptr into scheduler_, so a connection thread
    // reading scheduler_ as non-null during that window can still call into a
    // MatchScheduler that is mid-teardown on another thread. Found by
    // AddressSanitizer as a second, later crash at the exact same call site
    // this file's stop_all()/on_disconnect race already had one fix for: the
    // null check stopped the *already-null* case but not this one. The mutex
    // makes "take scheduler_ out and destroy it" and "look up scheduler_ and
    // call into it" mutually exclusive; the actual worker-thread joins still
    // happen with the mutex released (see stop_all()), so a slow teardown
    // never blocks the hot path this guards.
    std::mutex scheduler_mutex_;
    std::unique_ptr<MatchScheduler> scheduler_ = std::make_unique<MatchScheduler>();

    mutable std::mutex rooms_mutex_;
    std::map<RoomId, Room> rooms_;
    // Name -> room id, for the named Room feature (a room's name is its
    // identity, so it can't collide). Play (join_any) rooms are not named.
    std::map<std::string, RoomId> named_rooms_;
    // Matchmaking rooms with exactly one seat filled, ordered by that seat's
    // rating -- every Play-button room join_any might pair a newcomer with,
    // and nothing else: a named (Create/Join) room never enters this index,
    // and a matchmaking room leaves it the instant it pairs. join_any used to
    // find a compatible opponent by scanning every room in rooms_ -- Running,
    // Finished, named, all of them -- to filter down to the handful actually
    // waiting; at the five-million-room scale Server_Design.md targets, that
    // scan (and the single rooms_mutex_ every enqueue() also needs) is
    // exactly the lock cost this index exists to avoid. Sorted by rating, the
    // closest match to any newcomer is always one of the two entries
    // adjacent to lower_bound -- see closest_waiting_room -- so a lookup here
    // costs O(log w + 1) in the number of rooms actually waiting, never O(all
    // rooms).
    std::multimap<int, RoomId> waiting_by_rating_;
    RoomId next_room_id_ = 1;
};

}  // namespace kfc::server
