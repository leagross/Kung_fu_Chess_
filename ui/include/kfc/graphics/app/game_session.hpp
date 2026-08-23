#pragma once

#include <memory>
#include <optional>
#include <string>

#include "kfc/graphics/net/server_link.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/gameplay_config.hpp"
#include "kfc/realtime/piece_value_provider.hpp"
#include "kfc/texttests/game.hpp"
#include "kfc/texttests/game_view.hpp"

namespace kfc::graphics::app {

/// Owns whichever concrete game backend the command line selects (local kfc::texttests::Game or
/// networked net::ServerLink); both share the same gameplay.json config the server reads.
class GameSession {
public:
    /// Reads --server=ws://host:port from argv; without it, starts local single-player.
    GameSession(int argc, char** argv);

    /// True for a --server session (which must set_credentials() and connect() before view()).
    [[nodiscard]] bool is_networked() const {
        return networked_;
    }

    /// Only meaningful for a networked session; set from the Login dialog before connect().
    void set_credentials(std::string username, std::string password) {
        username_ = std::move(username);
        password_ = std::move(password);
    }

    [[nodiscard]] int board_width() const {
        return board_width_;
    }
    [[nodiscard]] int board_height() const {
        return board_height_;
    }

    /// Blocks up to 5s for the server's Welcome; returns false on timeout. A no-op returning true
    /// for local play. view() is valid only after this returns true.
    [[nodiscard]] bool connect(kfc::protocol::ClientMessage seating_action);

    /// Must be called before any blocking UI when the client stops searching, or the abandoned
    /// room stays a matchmaking candidate. A no-op for local play. view() is invalid afterwards.
    void disconnect();

    /// For networked play, true once MatchStart says both players are present; always true locally.
    [[nodiscard]] bool is_match_started() const {
        return !networked_ || (server_link_ != nullptr && server_link_->is_match_started());
    }

    /// Human-readable reason the last connect() failed, or a timeout message.
    [[nodiscard]] std::string join_failure_message() const;

    /// Empty for local play and Play matchmaking.
    [[nodiscard]] std::string room_name() const {
        return server_link_ != nullptr ? server_link_->room_name() : std::string{};
    }

    [[nodiscard]] std::string white_username() const {
        return server_link_ != nullptr ? server_link_->white_username() : std::string{};
    }
    [[nodiscard]] std::string black_username() const {
        return server_link_ != nullptr ? server_link_->black_username() : std::string{};
    }
    [[nodiscard]] int white_rating() const {
        return server_link_ != nullptr ? server_link_->white_rating() : 0;
    }
    [[nodiscard]] int black_rating() const {
        return server_link_ != nullptr ? server_link_->black_rating() : 0;
    }

    /// Arrivals before this client joined, replayed so a mid-game joiner's HUD matches the board.
    [[nodiscard]] std::vector<kfc::model::ArrivalEvent> history() const {
        return server_link_ != nullptr ? server_link_->history() : std::vector<kfc::model::ArrivalEvent>{};
    }

    /// A caller must not wire mouse input to view() when this holds.
    [[nodiscard]] bool is_spectator() const {
        return networked_ && server_link_ != nullptr && server_link_->is_spectator();
    }

    kfc::texttests::IGameView& view() {
        return *view_;
    }

    const kfc::model::IPieceValueProvider& value_provider() const {
        return *value_provider_;
    }

private:
    kfc::protocol::GameplayConfig gameplay_config_;
    std::optional<kfc::protocol::GameplayValueProvider> value_provider_;

    bool networked_ = false;
    std::string server_url_;
    std::string username_;
    std::string password_;
    int board_width_ = 0;
    int board_height_ = 0;

    std::unique_ptr<kfc::protocol::FileLogger> logger_;
    std::unique_ptr<net::ServerLink> server_link_;
    std::optional<std::string> join_failure_;  // kept from a failed connect(), which destroys the link

    // Must outlive local_game_, whose MotionFactory holds references into them.
    std::optional<kfc::protocol::GameplaySpeedProvider> speed_provider_;
    std::optional<kfc::protocol::GameplayCooldownPolicy> standard_cooldown_policy_;
    std::optional<kfc::protocol::GameplayCooldownPolicy> jump_cooldown_policy_;
    std::unique_ptr<kfc::texttests::Game> local_game_;

    kfc::texttests::IGameView* view_ = nullptr;
};

}  // namespace kfc::graphics::app
