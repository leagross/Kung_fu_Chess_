#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "kfc/engine/move_requester.hpp"
#include "kfc/events/event_bus.hpp"
#include "kfc/input/board_mapper.hpp"
#include "kfc/input/controller.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/messages.hpp"
#include "kfc/texttests/game_view.hpp"

namespace ix {
class WebSocket;
}

namespace kfc::graphics::net {

/// The networked counterpart to kfc::texttests::Game: implements IGameView
/// the same way, but every mutation is server-driven. board_ is a pure
/// mirror, replaced wholesale by Welcome and updated only by replaying each
/// BoardUpdate's ArrivalEvents; there is no local RealTimeArbiter.
///
/// motion_for() interpolates from predicted_motions_, which locally ticks
/// forward (clamped to duration_ms) once a MotionStarted arrives, purely so
/// PieceAnimator has something to draw before the real BoardUpdate confirms
/// the result; a prediction is discarded the moment its piece appears in an
/// arrival, matched or not.
///
/// Threading: IXWebSocket delivers on its own background thread. Incoming
/// messages are queued under a mutex and applied to board_ only from wait(),
/// called once per frame on the render thread, so every board mutation
/// happens on the thread that reads it. Welcome is awaited synchronously by
/// wait_for_welcome() before the render loop starts, so board_/
/// assigned_color_ need no synchronization after that.
class ServerLink : public kfc::texttests::IGameView, public kfc::model::IMoveRequester {
public:
    /// Connects asynchronously to server_url and sends Login once open.
    /// logger must outlive this ServerLink. Board access is invalid until
    /// wait_for_welcome() returns true. seating_action (Play/CreateRoom/
    /// JoinRoom) is sent right after Login.
    ServerLink(std::string server_url, std::string username, std::string password,
               kfc::protocol::ClientMessage seating_action, kfc::protocol::FileLogger& logger);
    ~ServerLink() override;

    ServerLink(const ServerLink&) = delete;
    ServerLink& operator=(const ServerLink&) = delete;

    /// Blocks (up to timeout_ms) for Welcome, building the internal
    /// Board/Controller from it. Returns false on timeout.
    [[nodiscard]] bool wait_for_welcome(int timeout_ms);

    /// Why the server refused to seat this connection, when wait_for_welcome()
    /// returned false. std::nullopt for a plain timeout (server unreachable).
    /// A rejected login also arrives here, prefixed with kLoginFailurePrefix.
    [[nodiscard]] std::optional<std::string> join_failure() const;

    /// Marks a join_failure() that was really an authentication failure.
    static constexpr const char* kLoginFailurePrefix = "login:";

    /// Valid only after wait_for_welcome(); meaningless for a spectator.
    [[nodiscard]] kfc::model::PieceColor assigned_color() const;

    /// True when seated as a viewer rather than a player; click()/jump() do
    /// nothing. Valid after wait_for_welcome().
    [[nodiscard]] bool is_spectator() const;

    /// Server-assigned room id (Welcome::room); empty for matchmaking.
    [[nodiscard]] const std::string& room_name() const;

    /// Arrivals this match produced before we joined, so a mid-game joiner
    /// can rebuild the move list and score it never saw.
    [[nodiscard]] const std::vector<kfc::model::ArrivalEvent>& history() const;

    /// True once MatchStart signals both players present; before that the
    /// client is seated but "searching".
    [[nodiscard]] bool is_match_started() const;

    // --- IGameView ---
    kfc::input::ControllerResult click(int x, int y) override;
    kfc::input::ControllerResult jump(int x, int y) override;
    /// Drains the incoming-message queue and applies it to board_. ms is
    /// unused; there is no local clock, only "what arrived since last frame".
    void wait(int ms) override;
    kfc::events::EventBus& events() override;
    const kfc::model::Board& board() const override;
    std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const override;
    /// Always false -- busy-checking is entirely the server's job now.
    bool is_piece_busy(kfc::model::PieceId piece_id) const override;

    // --- IMoveRequester ---
    /// Sends Move and returns optimistically without waiting for the
    /// server's answer; a rejection arrives later as MoveRejected.
    kfc::model::MoveResult request_move(const kfc::model::Position& source,
                                         const kfc::model::Position& destination) override;
    kfc::model::MoveResult request_jump(const kfc::model::Position& cell) override;

private:
    void on_message(const std::string& text);
    void apply_board_update(const kfc::protocol::BoardUpdate& update);
    void handle_motion_started(const kfc::protocol::MotionStarted& started);
    void send(const kfc::protocol::ClientMessage& message);
    // Rebuilds socket_ against url; called again by wait_for_welcome() on a
    // JoinRedirect. Caller must stop the old socket first.
    void connect_to(const std::string& url);

    std::string username_;
    std::string password_;
    kfc::protocol::ClientMessage seating_action_;
    kfc::protocol::FileLogger& logger_;
    std::unique_ptr<ix::WebSocket> socket_;

    // Welcome hand-off: written on the IXWebSocket thread, awaited by
    // wait_for_welcome() on the main thread before board_ is touched.
    mutable std::mutex welcome_mutex_;
    std::condition_variable welcome_cv_;
    std::optional<kfc::protocol::Welcome> pending_welcome_;
    std::optional<std::string> join_failure_;
    // Redirect to a different worker; guarded by welcome_mutex_ too.
    std::optional<std::string> pending_redirect_url_;

    // main-thread-only from here on (see wait())
    std::optional<kfc::model::Board> board_;
    std::optional<kfc::model::PieceColor> assigned_color_;
    bool spectator_ = false;
    std::string room_name_;
    // How far the game had got when Welcome's board was snapshotted; any
    // BoardUpdate at or below this is already reflected.
    std::uint64_t revision_ = 0;
    std::vector<kfc::model::ArrivalEvent> history_;
    std::optional<kfc::input::BoardMapper> board_mapper_;
    std::optional<kfc::input::Controller> controller_;
    struct PredictedMotion {
        kfc::model::Motion motion;
        // Wall-clock instant this motion actually started, so wait()
        // recomputes elapsed_ms as `now - started_at` every frame rather
        // than accumulating a per-frame delta -- accumulation silently
        // loses time whenever a frame's gap is clamped (e.g. this window
        // losing focus), drifting the two clients' animations apart.
        std::chrono::steady_clock::time_point started_at;
    };
    // Keyed by the same PieceId in both cases and always mutated in
    // lockstep, so one map avoids double-hashing every piece every frame.
    std::unordered_map<kfc::model::PieceId, PredictedMotion> predicted_motions_;

    std::mutex incoming_mutex_;
    std::vector<kfc::protocol::ServerMessage> incoming_queue_;

    kfc::events::EventBus events_;
    bool match_started_ = false;
};

}  // namespace kfc::graphics::net
