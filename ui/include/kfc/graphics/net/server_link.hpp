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

/// Networked counterpart to kfc::texttests::Game: board_ is a pure mirror replaying BoardUpdate's
/// ArrivalEvents, no local RealTimeArbiter; messages queue on IXWebSocket's thread, applied in wait().
class ServerLink : public kfc::texttests::IGameView, public kfc::model::IMoveRequester {
public:
    /// logger must outlive this ServerLink. Board access is invalid until wait_for_welcome() returns true.
    ServerLink(std::string server_url, std::string username, std::string password,
               kfc::protocol::ClientMessage seating_action, kfc::protocol::FileLogger& logger);
    ~ServerLink() override;

    ServerLink(const ServerLink&) = delete;
    ServerLink& operator=(const ServerLink&) = delete;

    /// Blocks (up to timeout_ms) for Welcome, building the internal Board/Controller from it.
    [[nodiscard]] bool wait_for_welcome(int timeout_ms);

    /// std::nullopt for a plain timeout; a rejected login is prefixed with kLoginFailurePrefix.
    [[nodiscard]] std::optional<std::string> join_failure() const;

    static constexpr const char* kLoginFailurePrefix = "login:";

    /// Valid only after wait_for_welcome(); meaningless for a spectator.
    [[nodiscard]] kfc::model::PieceColor assigned_color() const;

    /// True when seated as a viewer; click()/jump() do nothing.
    [[nodiscard]] bool is_spectator() const;

    /// Empty for matchmaking (no server-assigned room).
    [[nodiscard]] const std::string& room_name() const;

    /// Arrivals this match produced before we joined, so a mid-game joiner can rebuild the move list.
    [[nodiscard]] const std::vector<kfc::model::ArrivalEvent>& history() const;

    [[nodiscard]] const std::string& white_username() const {
        return white_username_;
    }
    [[nodiscard]] const std::string& black_username() const {
        return black_username_;
    }
    [[nodiscard]] int white_rating() const {
        return white_rating_;
    }
    [[nodiscard]] int black_rating() const {
        return black_rating_;
    }

    /// True once MatchStart signals both players present; before that the client is "searching".
    [[nodiscard]] bool is_match_started() const;

    // --- IGameView ---
    kfc::input::ControllerResult click(int x, int y) override;
    kfc::input::ControllerResult jump(int x, int y) override;
    /// Drains the incoming-message queue and applies it to board_; ms is unused (no local clock).
    void wait(int ms) override;
    kfc::events::EventBus& events() override;
    const kfc::model::Board& board() const override;
    std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const override;
    /// Always false -- busy-checking is entirely the server's job now.
    bool is_piece_busy(kfc::model::PieceId piece_id) const override;

    // --- IMoveRequester ---
    /// Returns optimistically without waiting for the server; a rejection arrives later as MoveRejected.
    kfc::model::MoveResult request_move(const kfc::model::Position& source,
                                         const kfc::model::Position& destination) override;
    kfc::model::MoveResult request_jump(const kfc::model::Position& cell) override;

private:
    void on_message(const std::string& text);
    void apply_board_update(const kfc::protocol::BoardUpdate& update);
    void handle_motion_started(const kfc::protocol::MotionStarted& started);
    void send(const kfc::protocol::ClientMessage& message);
    /// Called again by wait_for_welcome() on a JoinRedirect; caller must stop the old socket first.
    void connect_to(const std::string& url);

    std::string username_;
    std::string password_;
    kfc::protocol::ClientMessage seating_action_;
    kfc::protocol::FileLogger& logger_;
    std::unique_ptr<ix::WebSocket> socket_;

    // Welcome hand-off: written on the IXWebSocket thread, awaited by wait_for_welcome() on the main thread.
    mutable std::mutex welcome_mutex_;
    std::condition_variable welcome_cv_;
    std::optional<kfc::protocol::Welcome> pending_welcome_;
    std::optional<std::string> join_failure_;
    std::optional<std::string> pending_redirect_url_;  // guarded by welcome_mutex_ too

    // main-thread-only from here on (see wait())
    std::optional<kfc::model::Board> board_;
    std::optional<kfc::model::PieceColor> assigned_color_;
    bool spectator_ = false;
    std::string room_name_;
    // MatchStart overwrites both -- White's only way to learn Black's name/rating.
    std::string white_username_;
    std::string black_username_;
    int white_rating_ = 0;
    int black_rating_ = 0;
    std::uint64_t revision_ = 0;  // BoardUpdates at or below this are already reflected in board_
    std::vector<kfc::model::ArrivalEvent> history_;
    std::optional<kfc::input::BoardMapper> board_mapper_;
    std::optional<kfc::input::Controller> controller_;
    std::unordered_map<kfc::model::PieceId, kfc::model::Motion> predicted_motions_;
    // So wait() recomputes elapsed_ms as `now - this`, avoiding per-frame delta drift.
    std::unordered_map<kfc::model::PieceId, std::chrono::steady_clock::time_point> motion_start_times_;

    std::mutex incoming_mutex_;
    std::vector<kfc::protocol::ServerMessage> incoming_queue_;

    kfc::events::EventBus events_;
    bool match_started_ = false;
};

}  // namespace kfc::graphics::net
