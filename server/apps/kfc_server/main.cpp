// The kfc_server executable: top-level startup only. It loads the board and
// the shared gameplay config, builds a RoomManager (which opens a fresh Match
// per room on demand), hands it to a WebSocketGameServer (the transport), and
// runs it. Everything about *how* players connect lives in WebSocketGameServer,
// *which game* a player is in lives in RoomManager, and *what the game does*
// lives in Match.
#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "kfc/io/board_parser.hpp"
#include "kfc/model/board.hpp"
#include "kfc/model/piece.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/gameplay_config.hpp"
#include "kfc/database/rating_service.hpp"
#include "kfc/server/http_api.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/database/user_repository.hpp"
#include "kfc/server/session_registry.hpp"
#include "kfc/server/websocket_game_server.hpp"

namespace {

constexpr std::string_view kLogLevelFlag = "--log-level=";
constexpr std::string_view kHttpPortFlag = "--http-port=";

// A port number, or std::nullopt if the argument is not one. Written out rather
// than calling std::stoi directly, which throws on anything unparsable and
// would take the process down before the log file is even open.
std::optional<int> parse_port(const std::string& text) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos) {
        return std::nullopt;
    }
    try {
        int port = std::stoi(text);
        return (port > 0 && port <= 65535) ? std::optional<int>{port} : std::nullopt;
    } catch (const std::exception&) {
        return std::nullopt;  // out of int range
    }
}

// Set from a signal handler, so it must stay to what an atomic store can do
// -- no logging, no mutex, nothing that could deadlock if the signal lands
// mid-way through the very code it would need to call. main() polls this
// from an ordinary thread instead, which is where the actual shutdown runs
// (see the wait-then-stop loop below).
std::atomic<bool> g_shutdown_requested{false};

void request_shutdown(int /*signal*/) {
    g_shutdown_requested.store(true, std::memory_order_relaxed);
}

std::vector<std::string> read_board_lines(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open board file: " + path);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            lines.push_back(line);
        }
    }
    return lines;
}

}  // namespace

int main(int argc, char** argv) {
    int port = 8080;
    // The register/login/history HTTP API's port -- a second listening socket
    // (see http_api.hpp for why it can't share the WebSocket port).
    int http_port = 8081;
    // Everything by default, including the message-by-message traffic the spec
    // asks to be able to read afterwards. --log-level=info turns that traffic
    // off and leaves the events, for a long-running server where the dump is
    // more disk than it is worth.
    kfc::protocol::LogLevel log_level = kfc::protocol::LogLevel::Debug;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg.rfind(kLogLevelFlag, 0) == 0) {
            std::optional<kfc::protocol::LogLevel> parsed =
                kfc::protocol::FileLogger::level_from_name(arg.substr(kLogLevelFlag.size()));
            if (!parsed.has_value()) {
                std::cerr << "Unknown log level '" << arg.substr(kLogLevelFlag.size())
                          << "'. Expected one of: debug, info, warning, error.\n";
                return 1;
            }
            log_level = *parsed;
            continue;
        }
        if (arg.rfind(kHttpPortFlag, 0) == 0) {
            std::optional<int> parsed_http_port = parse_port(arg.substr(kHttpPortFlag.size()));
            if (!parsed_http_port.has_value()) {
                std::cerr << "Invalid --http-port value '" << arg.substr(kHttpPortFlag.size()) << "'\n";
                return 1;
            }
            http_port = *parsed_http_port;
            continue;
        }
        // The only positional argument. Rejected explicitly rather than left to
        // std::stoi, which would throw out of main on a typo.
        std::optional<int> parsed_port = parse_port(arg);
        if (!parsed_port.has_value()) {
            std::cerr << "Usage: kfc_server [port] [--http-port=8081] "
                         "[--log-level=debug|info|warning|error]\n";
            return 1;
        }
        port = *parsed_port;
    }

    kfc::protocol::FileLogger logger("kfc_server.log", log_level);
    logger.log("kfc_server starting on port " + std::to_string(port));

    try {
        std::vector<std::string> board_lines = read_board_lines(KFC_SERVER_DEFAULT_BOARD_FILE);
        // Parse once up front purely to fail fast on a bad board file, instead
        // of letting the first join hit the error lazily on a connection
        // thread. Each room then gets its own fresh Board from the factory.
        kfc::io::BoardParser().parse(board_lines);
        auto board_factory = [board_lines] { return kfc::io::BoardParser().parse(board_lines); };

        // The same gameplay.json the client loads, so networked and local
        // play agree on speed/cooldowns/pacing.
        kfc::protocol::GameplayConfig gameplay = kfc::protocol::load_gameplay_config(KFC_GAMEPLAY_CONFIG_FILE);

        // The account store: a single SQLite file in the working directory,
        // holding usernames, salted password hashes, and ELO ratings. Used both
        // to authenticate logins (WebSocketGameServer) and to apply each
        // finished game's rating change (the on_result hook below, run on a
        // match's tick thread -- UserRepository is internally synchronized).
        kfc::database::UserRepository users("kfc_users.db");
        auto on_result = [&users](kfc::server::GameEndReason reason, std::optional<kfc::model::PieceColor> winner,
                                  const std::string& white, const std::string& black,
                                  std::chrono::system_clock::time_point started_at) {
            if (reason == kfc::server::GameEndReason::Disconnect) {
                // The winner is the opponent, so the loser (who dropped) is the
                // other colour -- dock them the flat forfeit penalty.
                const std::string& loser = winner == kfc::model::PieceColor::White ? black : white;
                kfc::database::apply_forfeit(users, loser);
            } else {
                kfc::database::apply_game_result(users, winner, white, black);
            }

            std::optional<std::string> winner_username;
            if (winner.has_value()) {
                winner_username = *winner == kfc::model::PieceColor::White ? white : black;
            }
            const char* end_reason = reason == kfc::server::GameEndReason::Decisive  ? "decisive"
                                     : reason == kfc::server::GameEndReason::Draw    ? "draw"
                                                                                     : "disconnect";
            users.record_game(white, black, winner_username, end_reason, started_at,
                              std::chrono::system_clock::now());
        };

        kfc::server::RoomManager rooms(board_factory, logger, std::move(gameplay), on_result);

        // One account, one live connection (see SessionRegistry).
        kfc::server::SessionRegistry sessions;

        kfc::server::WebSocketGameServer server(port, rooms, users, sessions, logger);
        if (!server.listen()) {
            std::cerr << "Failed to listen on port " << port << " (see kfc_server.log)\n";
            return 1;
        }

        // Register/login/history -- a second, independent listening socket; see
        // http_api.hpp for why it can't share the WebSocket server's port.
        kfc::server::HttpApiServer http_server(http_port, users, logger);
        if (!http_server.listen()) {
            std::cerr << "Failed to listen on HTTP port " << http_port << " (see kfc_server.log)\n";
            return 1;
        }

        // Ctrl+C (SIGINT) and `docker stop` (SIGTERM) both used to hard-kill
        // the process -- nothing called server.stop(), so server.wait() below
        // never returned and the orderly teardown after it never ran. A
        // signal handler cannot safely do that stopping itself (it can only
        // set an atomic flag; server.stop() takes a mutex), so a small thread
        // polls the flag and does the actual stopping on our own time.
        std::signal(SIGINT, request_shutdown);
        std::signal(SIGTERM, request_shutdown);
        std::thread shutdown_watcher([&server] {
            while (!g_shutdown_requested.load(std::memory_order_relaxed)) {
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
            server.stop();  // unblocks server.wait() below
        });

        server.start();
        http_server.start();
        std::cout << "kfc_server listening on ws://localhost:" << port << ", http://localhost:" << http_port
                  << "\n";
        server.wait();
        shutdown_watcher.join();
        logger.log("kfc_server shutting down");

        // Shutdown order matters, and it is the opposite of the declaration
        // order, so it has to be spelled out rather than left to destructors.
        // Stopping the transport first leaves a frozen match's tick thread
        // broadcasting a disconnect countdown into the very sockets being closed
        // underneath it, and the process can hang with every game already over.
        // Rooms go quiet first; the socket layer comes down after, as this scope
        // exits. See RoomManager::stop_all. http_server has no match threads
        // behind it, so its own stop order is not load-bearing the same way.
        http_server.stop();
        rooms.stop_all();
    } catch (const std::exception& e) {
        logger.log(kfc::protocol::LogLevel::Error, std::string("Fatal: ") + e.what());
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
