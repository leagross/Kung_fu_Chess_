#include "../../../../include/kfc/graphics/app/game_session.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#include "kfc/graphics/constants.hpp"
#include "kfc/graphics/io/board_file_loader.hpp"
#include "kfc/io/board_parser.hpp"

namespace kfc::graphics::app {

namespace {

// Minimal "--name=value" parsing -- only two flags exist, not worth a
// library. Returns std::nullopt if flag_name isn't present at all.
std::optional<std::string> find_flag(int argc, char** argv, const std::string& flag_name) {
    std::string prefix = flag_name + "=";
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(prefix, 0) == 0) {
            return arg.substr(prefix.size());
        }
    }
    return std::nullopt;
}

}  // namespace

GameSession::GameSession(int argc, char** argv)
    : gameplay_config_(kfc::protocol::load_gameplay_config(KFC_GAMEPLAY_CONFIG_FILE)) {
    // The value provider (from the shared config) is available in both local
    // and networked play, for main()'s ScoreObserver.
    value_provider_.emplace(gameplay_config_);

    // --server=ws://host:port switches this app from local single-player to
    // a thin networked client -- see ServerLink's own class doc for why
    // board mutation is authoritative-server-driven and what that costs
    // visually in this phase. --username / --password are the shell login the
    // CTD SERVER spec asks for (entered on the command line, not through the
    // GUI): the server registers the pair on first use and requires a match
    // afterwards. Both default to "player" for a quick unattended local run.
    std::optional<std::string> server_url = find_flag(argc, argv, "--server");
    username_ = find_flag(argc, argv, "--username").value_or("player");
    password_ = find_flag(argc, argv, "--password").value_or("player");

    // The board is read from the default file in both modes: locally it's the
    // game board; for networked play it's discarded after providing the
    // dimensions the window layout needs before we ever connect (the server's
    // board is the same one). Only the connection itself is deferred to
    // connect(), so the Play button -- not app launch -- is what dials out.
    std::vector<std::string> board_lines = read_board_lines(default_board_file());
    kfc::model::Board initial_board = kfc::io::BoardParser().parse(board_lines);
    board_width_ = initial_board.width();
    board_height_ = initial_board.height();

    if (server_url.has_value()) {
        networked_ = true;
        server_url_ = *server_url;
        return;  // connect() dials out later, from the Play button
    }

    // Speed, cooldowns and pacing all come from gameplay_config_ -- the exact
    // same gameplay.json the server (server::Match) reads -- so a piece behaves
    // identically whether the game is local or networked. (Previously local
    // play read a per-sprite config.json speed while the server used a
    // different fixed default, so the same move took a different time in each
    // mode.)
    std::cout << "Loaded board: " << board_width_ << "x" << board_height_ << "\n";
    speed_provider_.emplace(gameplay_config_);
    standard_cooldown_policy_.emplace(gameplay_config_.standard_cooldown_ms);
    jump_cooldown_policy_.emplace(gameplay_config_.jump_cooldown_ms);

    local_game_ = std::make_unique<kfc::texttests::Game>(std::move(initial_board), *speed_provider_,
                                                           gameplay_config_.meters_per_cell,
                                                           *standard_cooldown_policy_, *jump_cooldown_policy_);
    view_ = local_game_.get();
}

bool GameSession::connect(kfc::protocol::ClientMessage seating_action) {
    if (view_ != nullptr) {
        return true;  // local play, or already connected
    }
    logger_ = std::make_unique<kfc::protocol::FileLogger>("kfc_gui_app.log");
    std::cout << "Connecting to " << server_url_ << " as '" << username_ << "'...\n";
    server_link_ =
        std::make_unique<net::ServerLink>(server_url_, username_, password_, std::move(seating_action), *logger_);
    if (!server_link_->wait_for_welcome(5000)) {
        // Keep *why* before dropping the link -- a refusal ("no such room",
        // "room not active") is a very different thing to tell the player than
        // a silent timeout, and the link is the only one who knows which it was.
        join_failure_ = server_link_->join_failure();
        server_link_.reset();
        return false;
    }
    std::cout << "Connected. Assigned color: "
              << (server_link_->assigned_color() == kfc::model::PieceColor::White ? "White" : "Black") << "\n";
    view_ = server_link_.get();
    return true;
}

std::string GameSession::join_failure_message() const {
    if (!join_failure_.has_value()) {
        // Nothing came back at all: the server isn't there, or isn't listening
        // on this address.
        return "Could not reach the server at " + server_url_ + ".";
    }
    const std::string& reason = *join_failure_;

    // An authentication failure, not a room problem -- see kLoginFailurePrefix.
    // Worth its own wording: telling someone their room doesn't exist when they
    // actually mistyped their password sends them looking in the wrong place.
    const std::string login_prefix = net::ServerLink::kLoginFailurePrefix;
    if (reason.rfind(login_prefix, 0) == 0) {
        std::string why = reason.substr(login_prefix.size());
        if (why == "wrong_password") {
            return "Wrong password for '" + username_ +
                   "'. That account already exists on this server -- use its password, or pick another username.";
        }
        return "Login failed for '" + username_ + "' (" + why + ").";
    }

    if (reason == kfc::protocol::join_reasons::kNoSuchRoom) {
        return "There is no room by that name. Check the name with the player who created it.";
    }
    if (reason == kfc::protocol::join_reasons::kRoomNotActive) {
        return "That room's game is already over, so it can no longer be joined.";
    }
    if (reason == kfc::protocol::join_reasons::kRoomNameTaken) {
        return "A room by that name already exists. Pick another name, or Join it instead.";
    }
    // An unrecognised reason still beats saying nothing -- and beats guessing.
    return "The server refused the request (" + reason + ").";
}

void GameSession::disconnect() {
    if (server_link_ == nullptr) {
        return;  // local play, or never connected
    }
    // Destroying the ServerLink closes the socket, which is what actually frees
    // the seat -- see the header for why that must not wait for process exit.
    view_ = nullptr;
    server_link_.reset();
}

}  // namespace kfc::graphics::app
