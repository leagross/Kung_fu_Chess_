#include "../../../../include/kfc/graphics/app/game_session.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#endif

#include "kfc/graphics/constants.hpp"
#include "kfc/graphics/io/board_file_loader.hpp"
#include "kfc/io/board_parser.hpp"
#include "kfc/model/piece_names.hpp"

namespace kfc::graphics::app {

namespace {

// Minimal "--name=value" parsing; returns std::nullopt if not present.
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

// Reads one line from the terminal without echoing it back.
std::string read_password_masked(const std::string& prompt) {
    std::cout << prompt << std::flush;
    std::string password;
#if defined(_WIN32)
    for (int ch = _getch(); ch != '\r' && ch != '\n'; ch = _getch()) {
        if (ch == '\b') {
            if (!password.empty()) {
                password.pop_back();
            }
            continue;
        }
        password.push_back(static_cast<char>(ch));
    }
#else
    termios original{};
    tcgetattr(STDIN_FILENO, &original);
    termios no_echo = original;
    no_echo.c_lflag &= ~static_cast<tcflag_t>(ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &no_echo);

    std::getline(std::cin, password);

    tcsetattr(STDIN_FILENO, TCSANOW, &original);
#endif
    std::cout << "\n";
    return password;
}

}  // namespace

GameSession::GameSession(int argc, char** argv)
    : gameplay_config_(kfc::protocol::load_gameplay_config(KFC_GAMEPLAY_CONFIG_FILE)) {
    value_provider_.emplace(gameplay_config_);

    std::optional<std::string> server_url = find_flag(argc, argv, "--server");
    username_ = find_flag(argc, argv, "--username").value_or("player");

    // Board is read from the default file in both modes: locally it's the
    // game board; for networked play it only provides dimensions before we
    // ever connect (the server's board is the same one).
    std::vector<std::string> board_lines = read_board_lines(default_board_file());
    kfc::model::Board initial_board = kfc::io::BoardParser().parse(board_lines);
    board_width_ = initial_board.width();
    board_height_ = initial_board.height();

    if (server_url.has_value()) {
        networked_ = true;
        server_url_ = *server_url;
        password_ = read_password_masked("Password for '" + username_ + "': ");
        return;  // connect() dials out later, from the Play button
    }

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
        // Capture why before dropping the link -- it's the only one that knows.
        join_failure_ = server_link_->join_failure();
        server_link_.reset();
        return false;
    }
    std::cout << "Connected. Assigned color: " << kfc::model::name_of(server_link_->assigned_color()) << "\n";
    view_ = server_link_.get();
    return true;
}

std::string GameSession::join_failure_message() const {
    if (!join_failure_.has_value()) {
        return "Could not reach the server at " + server_url_ + ".";
    }
    const std::string& reason = *join_failure_;

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
    return "The server refused the request (" + reason + ").";
}

void GameSession::disconnect() {
    if (server_link_ == nullptr) {
        return;  // local play, or never connected
    }
    view_ = nullptr;
    server_link_.reset();
}

}  // namespace kfc::graphics::app
