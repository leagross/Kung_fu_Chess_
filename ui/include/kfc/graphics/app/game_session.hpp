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

/// Owns whichever concrete game backend the command line selects -- local
/// single-player (kfc::texttests::Game, simulating in-process) or a networked
/// match (net::ServerLink) -- plus everything that backend depends on for its
/// own lifetime (FileLogger; the shared gameplay config and the providers
/// built from it for local play). Exists so main() only ever holds one
/// GameSession and reads view()/value_provider() from it, instead of juggling
/// several separate optional/unique_ptr members whose construction order and
/// lifetimes have to line up exactly (Game's MotionFactory holds long-lived
/// references into the speed provider/cooldown policies, not just at
/// construction -- see this class's own constructor for why they live here).
///
/// The gameplay config (speed, cooldowns, pacing, piece values) is loaded from
/// the very same gameplay.json the server reads, so a piece behaves the same
/// whether the game is local or networked.
class GameSession {
public:
    /// Reads --server=ws://host:port, --username and --password from argv;
    /// without --server, starts local single-player from the default board
    /// file. Determines the board dimensions immediately (from the default
    /// board file, which the networked board matches) so a caller can lay out
    /// its window before connecting, but for networked play does NOT connect
    /// yet -- call connect() for that, after the Play button. Throws
    /// std::runtime_error if the shared gameplay config or the board file can't
    /// be loaded.
    GameSession(int argc, char** argv);

    /// True for a --server session (which must connect() before view()).
    [[nodiscard]] bool is_networked() const {
        return networked_;
    }

    /// Board dimensions, known immediately in both modes (for window layout
    /// before a networked connection exists).
    [[nodiscard]] int board_width() const {
        return board_width_;
    }
    [[nodiscard]] int board_height() const {
        return board_height_;
    }

    /// For networked play, connects, authenticates, and sends seating_action
    /// (kfc::protocol::Play{} for matchmaking, or CreateRoom/JoinRoom for the
    /// named-room feature), then blocks up to 5s for the server's Welcome (which
    /// seats this player and builds the board/controller); returns false on
    /// timeout. A no-op returning true for local play. view() is valid only
    /// after this returns true.
    [[nodiscard]] bool connect(kfc::protocol::ClientMessage seating_action);

    /// Drops a networked connection immediately, giving up this player's seat on
    /// the server. Called when the client stops searching: the seat has to be
    /// released *before* any blocking UI (the "no opponent found" box), because
    /// until it is, this abandoned room is still a matchmaking candidate -- the
    /// next player to press Play gets seated opposite a player who is already
    /// gone, and sees a disconnect countdown the instant this process exits.
    /// A no-op for local play. view() is invalid afterwards.
    void disconnect();

    /// True once play should actually begin: for networked play, when the
    /// server's MatchStart says both players are present (before that the
    /// client is "searching"); always true for local play.
    [[nodiscard]] bool is_match_started() const {
        return !networked_ || (server_link_ != nullptr && server_link_->is_match_started());
    }

    /// A human-readable explanation of why the last connect() failed. Turns the
    /// server's machine-readable refusal (kfc::protocol::join_reasons) into a
    /// sentence to show the player, and falls back to the "couldn't reach the
    /// server" wording when the attempt simply timed out with no answer.
    [[nodiscard]] std::string join_failure_message() const;

    /// The room's id as the server assigned it, to display across the top of
    /// the screen. Empty for local play and for Play matchmaking. Valid after
    /// connect() returns true.
    [[nodiscard]] std::string room_name() const {
        return server_link_ != nullptr ? server_link_->room_name() : std::string{};
    }

    /// True when the server seated this connection as a viewer rather than a
    /// player -- a third or later joiner of a named room. Always false for local
    /// play. Valid only after connect() returns true; a caller must not wire
    /// mouse input to view() when this holds.
    [[nodiscard]] bool is_spectator() const {
        return networked_ && server_link_ != nullptr && server_link_->is_spectator();
    }

    kfc::texttests::IGameView& view() {
        return *view_;
    }

    /// The piece-value source (from the shared gameplay config) a caller's
    /// ScoreObserver should use, so scoring matches the same values the rest
    /// of the game is tuned with. Valid for both local and networked play.
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
    // Kept from a failed connect(), which destroys the link that knew it.
    std::optional<std::string> join_failure_;

    // Local play's motion providers, built from gameplay_config_ (the same
    // file the server uses). Must outlive local_game_, whose MotionFactory
    // holds references into them.
    std::optional<kfc::protocol::GameplaySpeedProvider> speed_provider_;
    std::optional<kfc::protocol::GameplayCooldownPolicy> standard_cooldown_policy_;
    std::optional<kfc::protocol::GameplayCooldownPolicy> jump_cooldown_policy_;
    std::unique_ptr<kfc::texttests::Game> local_game_;

    kfc::texttests::IGameView* view_ = nullptr;
};

}  // namespace kfc::graphics::app
