#pragma once

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
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

// SendFn / CloseFn live in their own header so MatchAudience can use them
// without depending on Match.

/// How a match ended. Decisive (king captured, resign) and Draw feed the
/// normal ELO exchange; Disconnect is a forfeit with a flat penalty instead
/// of a rating-dependent ELO swing.
enum class GameEndReason { Decisive, Draw, Disconnect };

/// Called once when a match is decided, so RoomManager can apply the rating
/// change. Match itself stays rating-agnostic; a Match built without one
/// (tests, local-only play) simply reports nothing.
using ResultCallback = std::function<void(GameEndReason reason, std::optional<kfc::model::PieceColor> winner,
                                          const std::string& white_username, const std::string& black_username,
                                          std::chrono::system_clock::time_point started_at)>;

/// What a match is doing right now, and therefore what is allowed. One
/// question, one answer, in one place -- every gate reads this instead of
/// separately checking game_over_/seats/disconnect state.
enum class MatchState {
    /// Fewer than two players seated; no gameplay command is accepted yet.
    Waiting,
    /// Both seats filled, nobody dropped: the game is on.
    Running,
    /// A player disconnected; countdown running, board frozen for both sides.
    Frozen,
    /// Decided; nothing changes the board again.
    Finished,
};

/// How long a dropped player has to come back before forfeiting.
inline constexpr int kDefaultDisconnectGraceMs = 20000;

/// How long after a match is decided its participants are released.
inline constexpr int kDefaultReleaseDelayMs = 3000;

/// Cap on queued-but-unapplied commands. A legitimate client can't move
/// faster than pieces come off cooldown, so this is generous headroom for a
/// catch-up burst while still bounding a flooding client's memory cost.
inline constexpr std::size_t kMaxQueuedCommands = 512;

/// Owns one playable match: a GameCore plus a thread-safe incoming-command
/// queue. Connection threads only ever call enqueue(); every mutation
/// happens inside tick(), on whichever thread a MatchScheduler assigns.
///
/// One Match is exactly two players (first join = White, second = Black),
/// no spectators seated, no persistence. Matchmaking across many concurrent
/// games lives one level up in RoomManager. Commands are applied in queue
/// order, deterministically.
class Match {
public:
    /// board is the starting position; logger must outlive this Match.
    /// config supplies shared gameplay timing (server and client load the
    /// same gameplay.json, so play agrees). on_result is called once the
    /// match is decided.
    explicit Match(kfc::model::Board board, kfc::protocol::FileLogger& logger,
                   kfc::protocol::GameplayConfig config = {}, ResultCallback on_result = {},
                   int disconnect_grace_ms = kDefaultDisconnectGraceMs,
                   int release_delay_ms = kDefaultReleaseDelayMs, std::string room_name = {});

    ~Match();

    Match(const Match&) = delete;
    Match& operator=(const Match&) = delete;

    /// Assigns the next open color (White first, then Black). Returns
    /// nullopt without registering anything if the match is already full.
    /// Immediately sends a Welcome with the current board snapshot.
    [[nodiscard]] std::optional<kfc::model::PieceColor> join(const std::string& username, int rating, SendFn send,
                                                              CloseFn close = {});

    /// Registers a watcher rather than a player: same Welcome/broadcasts as
    /// a player but no colour, never owns a piece, disconnect is not a
    /// forfeit. Returns 0 if MatchAudience::kMaxSpectators is already
    /// attached, in which case nothing is sent and the caller owes this
    /// connection a JoinFailed.
    [[nodiscard]] WatcherId join_spectator(const std::string& username, SendFn send, CloseFn close = {});

    /// Unknown handles are ignored.
    void leave_spectator(WatcherId watcher);

    /// Derived from decided/frozen/seated state rather than stored, so
    /// there's no separate copy of the truth to drift. Lock-free atomics:
    /// asked on every command and every tick.
    [[nodiscard]] MatchState state() const;

    /// Safe to call from any thread.
    [[nodiscard]] bool is_over() const;

    /// Colour of a player whose disconnect countdown is running and whose
    /// username this is; nullopt otherwise. Lets a returning player reclaim
    /// their seat while a stranger with the same room link becomes a
    /// spectator. Safe to call from a connection thread.
    [[nodiscard]] std::optional<kfc::model::PieceColor> reclaimable_seat_for(const std::string& username) const;

    /// Reseats a returning player and cancels the countdown. Returns false
    /// if the grace expired between reclaimable_seat_for saying yes and this
    /// call -- caller must undo whatever it did in anticipation.
    [[nodiscard]] bool reconnect(kfc::model::PieceColor color, SendFn send, CloseFn close = {});

    /// Thread-safe hand-off of one decoded message; applied on the next
    /// tick(). If a wake hook is set, it fires too so a scheduler ticks this
    /// match right away. Dropped (not queued) past kMaxQueuedCommands,
    /// newest-preferred, since older queued commands arrived first.
    void enqueue(kfc::model::PieceColor from, kfc::protocol::ClientMessage message);

    /// Starts a disconnect grace countdown instead of ending the game
    /// immediately. Called from a connection thread; just records the event
    /// for the next tick(), never races the simulation.
    void on_disconnect(kfc::model::PieceColor color);

    /// Drains the command queue, advances the simulation by elapsed_ms, and
    /// broadcasts what happened. now/elapsed_ms come in as parameters
    /// (not read from a clock) so one thread can drive many matches and
    /// tests can step deterministically.
    ///
    /// Safe to call from any one thread at a time; two threads must not
    /// tick the same match concurrently.
    void tick(std::chrono::steady_clock::time_point now, int elapsed_ms);

    /// Callback enqueue() fires after queuing a command, so a MatchScheduler
    /// can tick this match now instead of waiting its normal cadence.
    /// Expected to be set once, before the match is reachable concurrently.
    void set_wake_hook(std::function<void()> hook);

private:
    void apply(kfc::model::PieceColor from, const kfc::protocol::ClientMessage& message);
    /// True unless cell holds an opponent's piece.
    [[nodiscard]] bool owns_piece_at(kfc::model::PieceColor from, const kfc::model::Position& cell) const;
    [[nodiscard]] kfc::protocol::Welcome welcome_for(kfc::model::PieceColor color, bool spectator) const;
    /// Fires on_result_ once (guarded by result_reported_).
    void report_result(GameEndReason reason, std::optional<kfc::model::PieceColor> winner);
    void advance_disconnect_countdown(std::chrono::steady_clock::time_point now);
    /// Closes every participant's connection once, shortly after the match
    /// is decided, which also lets the room be reaped.
    void release_participants();
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

    // Bumped once per tick producing arrivals, under board_mutex_, so a
    // snapshot and revision taken under that lock always agree.
    std::uint64_t revision_ = 0;

    // Every arrival this match has produced, oldest first. Written by the
    // tick thread, read by welcome_for on a connection thread, both under
    // board_mutex_.
    std::vector<kfc::model::ArrivalEvent> history_;

    // join() runs on a connection thread and reads the board for its Welcome
    // snapshot, which can otherwise race the tick thread's mutation inside
    // engine().wait().
    mutable std::mutex board_mutex_;

    kfc::protocol::FileLogger& logger_;

    // Internally synchronized; Match needs no lock of its own for it.
    MatchAudience audience_;

    std::mutex queue_mutex_;
    std::deque<std::pair<kfc::model::PieceColor, kfc::protocol::ClientMessage>> queue_;
    // Commands dropped since last reported, so a flood costs one log line, not one per message.
    int dropped_commands_ = 0;

    // Set once before the match is reachable from more than one thread, so
    // enqueue() (any connection thread) reads it without a lock.
    std::function<void()> wake_hook_;

    // tick-thread-only game-over state; game_over_ latches so a stray
    // command after the win never mutates a finished board. Atomic only so
    // is_over() can be asked from a connection thread. pending_game_over_ is
    // consumed by tick() after releasing board_mutex_, since network I/O
    // must not hold the board lock.
    std::atomic<bool> game_over_{false};
    std::optional<kfc::protocol::GameOver> pending_game_over_;

    // tick-thread-only. result_reported_ guards the one-shot rating hook.
    ResultCallback on_result_;
    bool result_reported_ = false;

    std::chrono::system_clock::time_point started_at_;
    int release_delay_ms_;

    // Empty for a matchmaking room. Set once at construction.
    std::string room_name_;

    DisconnectWatch disconnect_watch_;

    // tick-thread-only. Set a short grace after the match is decided so the
    // final GameOver reaches both screens first; released_ makes it happen once.
    std::optional<std::chrono::steady_clock::time_point> release_at_;
    bool released_ = false;
};

}  // namespace kfc::server
