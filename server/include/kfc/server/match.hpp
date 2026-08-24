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

/// Disconnect forfeits at a flat penalty; Decisive/Draw use normal ELO.
enum class GameEndReason { Decisive, Draw, Disconnect };

/// Called once when a match is decided; unset for rating-agnostic matches (tests, local play).
using ResultCallback = std::function<void(GameEndReason reason, std::optional<kfc::model::PieceColor> winner,
                                          const std::string& white_username, const std::string& black_username,
                                          std::chrono::system_clock::time_point started_at)>;

/// What a match is doing right now; every gate reads this instead of raw state.
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

/// Cap on queued-but-unapplied commands; headroom for a catch-up burst while bounding a flooding client.
inline constexpr std::size_t kMaxQueuedCommands = 512;

/// Owns one playable match. Connection threads only call enqueue(); every mutation happens in tick().
class Match {
public:
    /// logger must outlive this Match; config must match the client's gameplay.json.
    explicit Match(kfc::model::Board board, kfc::protocol::FileLogger& logger,
                   kfc::protocol::GameplayConfig config = {}, ResultCallback on_result = {},
                   int disconnect_grace_ms = kDefaultDisconnectGraceMs,
                   int release_delay_ms = kDefaultReleaseDelayMs, std::string room_name = {});

    ~Match();

    Match(const Match&) = delete;
    Match& operator=(const Match&) = delete;

    /// Assigns the next open color (White first, Black second); nullopt if already full.
    [[nodiscard]] std::optional<kfc::model::PieceColor> join(const std::string& username, int rating, SendFn send,
                                                              CloseFn close = {});

    /// Returns 0 (no JoinFailed sent by caller's contract) if MatchAudience::kMaxSpectators is already attached.
    [[nodiscard]] WatcherId join_spectator(const std::string& username, SendFn send, CloseFn close = {});

    void leave_spectator(WatcherId watcher);

    [[nodiscard]] MatchState state() const;

    [[nodiscard]] bool is_over() const;

    /// Colour of a player mid-disconnect-countdown under this username, so they can reclaim their seat.
    [[nodiscard]] std::optional<kfc::model::PieceColor> reclaimable_seat_for(const std::string& username) const;

    /// False if the grace expired between reclaimable_seat_for saying yes and this call.
    [[nodiscard]] bool reconnect(kfc::model::PieceColor color, SendFn send, CloseFn close = {});

    /// Dropped past kMaxQueuedCommands, newest-preferred; wakes the scheduler if a wake hook is set.
    void enqueue(kfc::model::PieceColor from, kfc::protocol::ClientMessage message);

    /// Starts the disconnect grace countdown instead of ending the game immediately.
    void on_disconnect(kfc::model::PieceColor color);

    /// now/elapsed_ms are parameters, not read from a clock, so one thread can drive many matches.
    void tick(std::chrono::steady_clock::time_point now, int elapsed_ms);

    void set_wake_hook(std::function<void()> hook);

private:
    void apply(kfc::model::PieceColor from, const kfc::protocol::ClientMessage& message);
    /// True unless cell holds an opponent's piece.
    [[nodiscard]] bool owns_piece_at(kfc::model::PieceColor from, const kfc::model::Position& cell) const;
    [[nodiscard]] kfc::protocol::Welcome welcome_for(kfc::model::PieceColor color, bool spectator) const;
    /// Fires on_result_ once (guarded by result_reported_).
    void report_result(GameEndReason reason, std::optional<kfc::model::PieceColor> winner);
    void advance_disconnect_countdown(std::chrono::steady_clock::time_point now);
    void release_participants();
    void broadcast_and_log(const kfc::protocol::ServerMessage& message);
    void send_to_and_log(kfc::model::PieceColor color, const kfc::protocol::ServerMessage& message);

    // Declaration order matters: providers reference config_, and core_'s MotionFactory references the providers.
    kfc::protocol::GameplayConfig config_;
    kfc::protocol::GameplaySpeedProvider speed_provider_;
    kfc::protocol::GameplayCooldownPolicy standard_policy_;
    kfc::protocol::GameplayCooldownPolicy jump_policy_;
    kfc::model::GameCore core_;

    // Bumped per tick producing arrivals, under board_mutex_, so a snapshot and revision always agree.
    std::uint64_t revision_ = 0;

    // Written by the tick thread, read by welcome_for on a connection thread, both under board_mutex_.
    std::vector<kfc::model::ArrivalEvent> history_;

    // Guards against join()'s Welcome snapshot racing the tick thread's mutation.
    mutable std::mutex board_mutex_;

    kfc::protocol::FileLogger& logger_;

    MatchAudience audience_;

    std::mutex queue_mutex_;
    std::deque<std::pair<kfc::model::PieceColor, kfc::protocol::ClientMessage>> queue_;
    int dropped_commands_ = 0;

    std::function<void()> wake_hook_;

    // Latches so a stray command after the win never mutates a finished board.
    std::atomic<bool> game_over_{false};
    // Consumed by tick() after releasing board_mutex_; network I/O must not hold the board lock.
    std::optional<kfc::protocol::GameOver> pending_game_over_;

    ResultCallback on_result_;
    bool result_reported_ = false;

    std::chrono::system_clock::time_point started_at_;
    int release_delay_ms_;

    std::string room_name_;

    DisconnectWatch disconnect_watch_;

    // Short grace after the match is decided so the final GameOver reaches both screens first.
    std::optional<std::chrono::steady_clock::time_point> release_at_;
    bool released_ = false;
};

}  // namespace kfc::server
