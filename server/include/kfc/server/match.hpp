#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "kfc/engine/game_core.hpp"
#include "kfc/model/board.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/gameplay_config.hpp"
#include "kfc/protocol/messages.hpp"
#include "kfc/server/connection_callbacks.hpp"
#include "kfc/server/disconnect_watch.hpp"
#include "kfc/server/match_audience.hpp"

namespace kfc::server {

// SendFn / CloseFn -- how the server reaches and releases one connection --
// live in their own header, so MatchAudience can use them without depending on
// Match.

/// How a match ended -- it decides how ratings move. A Decisive result (a king
/// captured, or a deliberate resign) and a Draw feed the normal ELO exchange;
/// a Disconnect is a forfeit that just docks the loser a flat penalty (see the
/// CTD SERVER spec's -10), never a rating-dependent ELO swing.
enum class GameEndReason { Decisive, Draw, Disconnect };

/// Called exactly once when a match is decided: how it ended, the winner
/// (std::nullopt for a draw), and both players' usernames. This is the hook
/// RoomManager uses to apply the rating change (ELO or forfeit penalty); Match
/// itself stays rating-agnostic, so a Match built without one (tests,
/// local-only play) simply reports nothing.
using ResultCallback = std::function<void(GameEndReason reason, std::optional<kfc::model::PieceColor> winner,
                                          const std::string& white_username, const std::string& black_username)>;

/// What a match is doing right now, and therefore what is allowed.
///
/// This exists because "may this command be applied?" used to be answered by
/// reading several unrelated flags -- game_over_, the seat table, the disconnect
/// watch -- and it was easy to check some and forget others. It was: a player
/// waiting alone for an opponent could move their pieces, because nothing
/// checked that anyone was there to move against.
///
/// One question, one answer, in one place. Every gate reads this.
enum class MatchState {
    /// Fewer than two players are seated. The board exists, but no gameplay
    /// command is accepted -- there is no game yet.
    Waiting,
    /// Both seats filled and nobody has dropped: the game is on.
    Running,
    /// A player's connection dropped and their grace period is counting down.
    /// The clock is stopped and no move is accepted from anyone, so a returning
    /// player finds the position exactly as they left it.
    Frozen,
    /// Decided -- a king fell, someone resigned, or a grace period expired.
    /// Nothing changes the board again.
    Finished,
};

/// How long a dropped player has to come back before the match is forfeited on
/// their behalf -- the CTD SERVER spec's 20 seconds.
inline constexpr int kDefaultDisconnectGraceMs = 20000;

/// How long after a match is decided its participants are released. Long enough
/// for the final GameOver to reach every screen and register there.
inline constexpr int kDefaultReleaseDelayMs = 3000;

/// Owns one playable match: a GameCore (the very same Board/RuleEngine/
/// RealTimeArbiter/MotionFactory/GameEngine stack kfc::texttests::Game
/// assembles locally, now shared rather than re-wired by hand), plus a
/// dedicated tick thread and a thread-safe incoming-command queue. This
/// *is* the CTD SERVER lecture's "bus": IXWebSocket's own connection
/// threads only ever call enqueue(), never touch GameEngine/Board directly
/// -- every mutation happens on tick_loop's single thread, exactly the
/// single-threaded assumption kfc_core makes everywhere already.
///
/// One Match is exactly two players (first join = White, second = Black), no
/// spectators, no persistence. Matchmaking across many concurrent games lives
/// one level up in RoomManager, which owns a Match per room -- a Match itself
/// never needs to know other games exist. Commands are applied in the order
/// the queue delivers them -- deterministic, and answers the lecture's own
/// "which command wins on a near-tie" question without any special-casing.
class Match {
public:
    /// board is the starting position; logger must outlive this Match.
    /// config supplies the shared gameplay timing/values (speed, cooldowns,
    /// meters-per-cell) -- the server loads it from gameplay.json and the
    /// client loads the same file, so networked and local play agree. Defaults
    /// to the built-in values, which is what tests use.
    /// on_result, if set, is called once the moment the match is decided --
    /// see ResultCallback. Defaults to none, which is what tests and any
    /// rating-free Match use. disconnect_grace_ms is how long a dropped player
    /// has before the match is forfeited on their behalf (CTD SERVER spec: 20
    /// seconds); tests pass a short value so the countdown resolves quickly.
    /// release_delay_ms is how long after the match is decided both players are
    /// let go (see release_players) -- long enough by default for the final
    /// GameOver to be seen; tests again pass a short value.
    explicit Match(kfc::model::Board board, kfc::protocol::FileLogger& logger,
                   kfc::protocol::GameplayConfig config = {}, ResultCallback on_result = {},
                   int disconnect_grace_ms = kDefaultDisconnectGraceMs,
                   int release_delay_ms = kDefaultReleaseDelayMs, std::string room_name = {});

    /// Stops the tick thread if still running (see stop()).
    ~Match();

    Match(const Match&) = delete;
    Match& operator=(const Match&) = delete;

    /// Registers a new connection's send callback and assigns it the next
    /// open color (White first, then Black). Returns std::nullopt without
    /// registering anything if the match is already full -- the caller
    /// (kfc_server's connection handler) is responsible for rejecting that
    /// connection instead. Immediately sends that player a Welcome message
    /// with the current board snapshot. close, if given, is how this player is
    /// released once the match is decided (see release_players); a Match built
    /// without one simply leaves its players connected, which is what tests do.
    [[nodiscard]] std::optional<kfc::model::PieceColor> join(const std::string& username, SendFn send,
                                                              CloseFn close = {});

    /// Registers a watcher rather than a player -- the spec's "the following
    /// people who join are viewers". A spectator gets the same Welcome (marked
    /// spectator, with the board as it stands *right now*, not as it started)
    /// and every later broadcast, so its screen mirrors the game exactly; it is
    /// never given a colour, never asked for by owns_piece_at, and its
    /// disconnect is not a forfeit -- nothing about the game changes when one
    /// comes or goes. If the match is already under way it is also sent
    /// MatchStart immediately, so it starts watching instead of "searching".
    /// Unlimited: seats are what's capped at two, not viewers.
    void join_spectator(const std::string& username, SendFn send, CloseFn close = {});

    /// What this match is doing right now -- see MatchState. Derived from the
    /// three things that actually decide it (decided? frozen? both seated?)
    /// rather than stored, so there is no fourth copy of the truth to drift.
    ///
    /// All three reads are lock-free atomics, because this is asked for every
    /// command and every tick.
    [[nodiscard]] MatchState state() const;

    /// True once this match is decided (a king fell, someone resigned, or a
    /// disconnect grace ran out). Nothing can be joined, watched or played in
    /// such a room, so callers refuse to seat anyone into one -- see
    /// kfc::protocol::join_reasons::kRoomNotActive. Safe to call from any
    /// thread.
    [[nodiscard]] bool is_over() const;

    /// Which seat, if any, this username is entitled to reclaim right now: the
    /// colour of a player whose disconnect countdown is currently running and
    /// whose username this is. std::nullopt in every other case -- nobody is
    /// mid-countdown, or this is simply a different person. This is what keeps
    /// a returning player apart from a stranger walking into the same room: a
    /// stranger becomes a spectator, the actual player gets their pieces back.
    /// Safe to call from a connection thread.
    [[nodiscard]] std::optional<kfc::model::PieceColor> reclaimable_seat_for(const std::string& username) const;

    /// Reseats a returning player: swaps in their new connection's send/close,
    /// cancels the countdown, tells the opponent (OpponentReconnected) to clear
    /// it, and sends the returning player a Welcome with the board as it stands
    /// now plus MatchStart, so they resume the game already in progress. Only
    /// valid for a colour reclaimable_seat_for just returned.
    void reconnect(kfc::model::PieceColor color, SendFn send, CloseFn close = {});

    /// Thread-safe hand-off of one decoded client message, called from
    /// whatever IXWebSocket callback thread received it. Returns
    /// immediately; the message is applied on tick_loop's own thread.
    void enqueue(kfc::model::PieceColor from, kfc::protocol::ClientMessage message);

    /// A player's connection dropped: starts a disconnect grace countdown
    /// rather than ending the game immediately. Each remaining second is
    /// broadcast to the opponent (OpponentDisconnected) so their screen can
    /// count it down; if the grace elapses the match is forfeited on the
    /// dropped player's behalf (opponent wins, GameEndReason::Disconnect, the
    /// flat -10 penalty). Called from an IXWebSocket connection thread when a
    /// joined socket closes; like enqueue(), it just hands the event to the
    /// tick thread and returns, so it never races the simulation.
    void on_disconnect(kfc::model::PieceColor color);

    /// Starts the dedicated tick thread (advances real time and drains the
    /// command queue every iteration). No-op if already running.
    void start();

    /// Signals the tick thread to stop and joins it. No-op if not running.
    void stop();

private:
    void tick_loop();
    void apply(kfc::model::PieceColor from, const kfc::protocol::ClientMessage& message);
    /// True if cell is empty or holds a piece of color from -- false only
    /// when it holds an opponent's piece, i.e. from is trying to command a
    /// piece that isn't theirs. See apply()'s own use for why an empty cell
    /// is not treated as a violation here.
    [[nodiscard]] bool owns_piece_at(kfc::model::PieceColor from, const kfc::model::Position& cell) const;
    /// The Welcome to hand a newly attached connection: its colour, whether it
    /// is watching, this room's id, and a snapshot of the board as it stands
    /// right now. Shared by join/join_spectator/reconnect, which differ only in
    /// the first two.
    [[nodiscard]] kfc::protocol::Welcome welcome_for(kfc::model::PieceColor color, bool spectator) const;
    /// Fires on_result_ once (guarded by result_reported_) with the end reason,
    /// winner, and the two usernames. Called from tick_loop the tick the game is
    /// decided, whichever way it ended (capture, resign, disconnect).
    void report_result(GameEndReason reason, std::optional<kfc::model::PieceColor> winner);
    /// Turns this tick of the disconnect grace into game state: broadcasts each
    /// remaining second as it changes, and forfeits the match if the grace ran
    /// out. The countdown itself lives in DisconnectWatch; this is only what
    /// Match does about it. `now` is this tick's clock.
    void advance_disconnect_countdown(std::chrono::steady_clock::time_point now);
    /// Closes every participant's connection, once, a moment after the match is
    /// decided -- see the release_at_ field. Nobody is still playing in a
    /// finished room, and the survivor of a disconnect forfeit in particular
    /// would otherwise sit in it forever; letting everyone go is also what lets
    /// the room be reaped (each close comes back as an ordinary disconnect).
    void release_participants();
    /// Encodes and logs one message, then hands the text to audience_. The two
    /// wrappers exist so encoding and its log line live in one place rather
    /// than at every call site.
    void broadcast_and_log(const kfc::protocol::ServerMessage& message);
    void send_to_and_log(kfc::model::PieceColor color, const kfc::protocol::ServerMessage& message);

    // config_ must precede the providers, which must precede core_: the
    // providers reference config_, and core_'s MotionFactory references the
    // providers (construction follows declaration order, not init-list order).
    kfc::protocol::GameplayConfig config_;
    kfc::protocol::GameplaySpeedProvider speed_provider_;
    kfc::protocol::GameplayCooldownPolicy standard_policy_;
    kfc::protocol::GameplayCooldownPolicy jump_policy_;
    kfc::model::GameCore core_;

    // How far the game has got: bumped once per tick that produces arrivals,
    // under board_mutex_ together with the board change itself, so a snapshot
    // and a revision taken under that lock always agree. Sent with every
    // BoardUpdate and every Welcome -- see BoardUpdate::revision.
    std::uint64_t revision_ = 0;

    // Every arrival this match has produced, oldest first -- what a joining or
    // returning client needs to rebuild the move list and score it never saw.
    // Written by the tick thread and read by welcome_for on a connection
    // thread, both under board_mutex_ below, exactly like the board itself.
    std::vector<kfc::model::ArrivalEvent> history_;

    // Serializes every read of / write to core_'s board across threads.
    // Nearly all board access already happens on tick_loop's own thread, but
    // join() runs on an IXWebSocket connection thread and reads the board to
    // build its Welcome snapshot -- without this, that read can race a
    // simultaneous mutation the tick thread is making inside engine().wait().
    mutable std::mutex board_mutex_;

    kfc::protocol::FileLogger& logger_;

    // Who is attached to this match and how to reach them -- players and
    // watchers alike. Internally synchronized, so Match needs no lock of its
    // own for any of it (see MatchAudience).
    MatchAudience audience_;

    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::deque<std::pair<kfc::model::PieceColor, kfc::protocol::ClientMessage>> queue_;

    std::thread tick_thread_;
    std::atomic<bool> running_{false};

    // tick-thread-only game-over state. Once a king is captured, a player
    // resigns, or a player disconnects, the match is decided: game_over_ stays
    // true and apply() drops every later command, so a stray move arriving
    // after the win can never mutate a finished board or emit a second result.
    // Only apply()/tick_loop touch these, both on the tick thread, so no mutex
    // is needed (unlike the board or the player table, which connection threads
    // also read). pending_game_over_ is set the tick a resign/disconnect is
    // applied and consumed by tick_loop, which does the actual broadcast after
    // releasing board_mutex_ -- network I/O must not hold the board lock.
    // atomic only so is_over() can be asked from a connection thread deciding
    // whether a room is still joinable; every write is still the tick thread's.
    std::atomic<bool> game_over_{false};
    std::optional<kfc::protocol::GameOver> pending_game_over_;

    // The rating hook and a one-shot guard so a result is reported exactly once
    // even though game-over is latched on the tick thread. tick-thread-only.
    ResultCallback on_result_;
    bool result_reported_ = false;

    // How long after the match is decided every participant is released.
    int release_delay_ms_;

    // This room's id, echoed in every Welcome so a client can display it (the
    // spec's "the room id is written on the top of the screen"). Empty for a
    // matchmaking room, which has no id to show. Set once at construction and
    // never written again, so it needs no lock.
    std::string room_name_;

    // The dropped-player grace period, start to finish -- reporting a drop,
    // counting it down, cancelling it on a reconnect, and expiring it into a
    // forfeit. Internally synchronized (see DisconnectWatch).
    DisconnectWatch disconnect_watch_;

    // When everyone is let go (tick-thread-only). Set the tick the match is
    // decided, a short grace *after* it so the final GameOver has reached both
    // screens before their sockets close; released_ makes it happen once.
    std::optional<std::chrono::steady_clock::time_point> release_at_;
    bool released_ = false;
};

}  // namespace kfc::server
