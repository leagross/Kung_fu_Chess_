// The kfc_server executable: top-level startup only.
#include <atomic>
#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <memory>
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
#include "kfc/server/metrics.hpp"
#include "kfc/server/rate_limiter.hpp"
#include "kfc/server/redis_room_directory.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/database/user_repository.hpp"
#include "kfc/server/session_registry.hpp"
#include "kfc/server/websocket_game_server.hpp"

namespace {

constexpr std::string_view kLogLevelFlag = "--log-level=";
constexpr std::string_view kHttpPortFlag = "--http-port=";
constexpr std::string_view kRedisHostFlag = "--redis-host=";
constexpr std::string_view kRedisPortFlag = "--redis-port=";
constexpr std::string_view kWorkerUrlFlag = "--worker-url=";
constexpr int kDefaultRedisPort = 6379;

// Avoids std::stoi's throw-on-unparsable, which would take the process down
// before the log file is even open.
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

// Set from a signal handler: an atomic store only, nothing that could
// deadlock mid-signal. main() polls this from an ordinary thread instead.
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
    int http_port = 8081;
    // Debug logs message-by-message traffic; --log-level=info turns that off.
    kfc::protocol::LogLevel log_level = kfc::protocol::LogLevel::Debug;
    // Unset means single-worker mode: no IRoomDirectory. Set together with
    // --worker-url, this worker shares room lookup across others via Redis.
    std::string redis_host;
    int redis_port = kDefaultRedisPort;
    std::string worker_url;

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
        if (arg.rfind(kRedisHostFlag, 0) == 0) {
            redis_host = arg.substr(kRedisHostFlag.size());
            continue;
        }
        if (arg.rfind(kRedisPortFlag, 0) == 0) {
            std::optional<int> parsed_redis_port = parse_port(arg.substr(kRedisPortFlag.size()));
            if (!parsed_redis_port.has_value()) {
                std::cerr << "Invalid --redis-port value '" << arg.substr(kRedisPortFlag.size()) << "'\n";
                return 1;
            }
            redis_port = *parsed_redis_port;
            continue;
        }
        if (arg.rfind(kWorkerUrlFlag, 0) == 0) {
            worker_url = arg.substr(kWorkerUrlFlag.size());
            continue;
        }
        std::optional<int> parsed_port = parse_port(arg);
        if (!parsed_port.has_value()) {
            std::cerr << "Usage: kfc_server [port] [--http-port=8081] [--redis-host=host] "
                         "[--redis-port=6379] [--worker-url=ws://host:port] "
                         "[--log-level=debug|info|warning|error]\n";
            return 1;
        }
        port = *parsed_port;
    }

    if (!redis_host.empty() && worker_url.empty()) {
        std::cerr << "--redis-host requires --worker-url (this worker's own client-facing address, "
                     "e.g. ws://localhost:8082) -- otherwise a room it creates could never be redirected to.\n";
        return 1;
    }

    kfc::protocol::FileLogger logger("kfc_server.log", log_level);
    logger.log("kfc_server starting on port " + std::to_string(port));

    try {
        std::vector<std::string> board_lines = read_board_lines(KFC_SERVER_DEFAULT_BOARD_FILE);
        // Parsed once up front to fail fast on a bad board file.
        kfc::io::BoardParser().parse(board_lines);
        auto board_factory = [board_lines] { return kfc::io::BoardParser().parse(board_lines); };

        kfc::protocol::GameplayConfig gameplay = kfc::protocol::load_gameplay_config(KFC_GAMEPLAY_CONFIG_FILE);

        kfc::database::UserRepository users("kfc_users.db");
        auto on_result = [&users](kfc::server::GameEndReason reason, std::optional<kfc::model::PieceColor> winner,
                                  const std::string& white, const std::string& black,
                                  std::chrono::system_clock::time_point started_at) {
            if (reason == kfc::server::GameEndReason::Disconnect) {
                // Loser is the dropped player, i.e. the opposite of the winner.
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

        // Declared here (not inside the if) so it outlives rooms below, which only borrows the pointer.
        std::unique_ptr<kfc::server::RedisRoomDirectory> room_directory;
        if (!redis_host.empty()) {
            room_directory = std::make_unique<kfc::server::RedisRoomDirectory>(redis_host, redis_port);
            logger.log("kfc_server: room directory at " + redis_host + ":" + std::to_string(redis_port) +
                       ", advertising this worker as " + worker_url);
        }

        // Declared before rooms, which borrows it for its own lifetime.
        kfc::server::Metrics metrics;

        // Shared budget across register/login/WebSocket-Login (see http_api.hpp).
        kfc::server::RateLimiter auth_limiter(10, std::chrono::minutes(1));

        // A separate budget from auth_limiter's, for Play/CreateRoom/JoinRoom
        // (see join_reasons::kRateLimited). 30/minute is generous for a real
        // player while still bounding how fast one IP can fill a room's
        // spectator slots with fresh connections.
        kfc::server::RateLimiter seat_limiter(30, std::chrono::minutes(1));

        kfc::server::RoomManager rooms(board_factory, logger, std::move(gameplay), on_result,
                                       kfc::server::kDefaultDisconnectGraceMs, room_directory.get(), worker_url,
                                       &metrics);

        kfc::server::SessionRegistry sessions;

        kfc::server::WebSocketGameServer server(port, rooms, users, sessions, logger, &metrics, &auth_limiter,
                                                 &seat_limiter);
        if (!server.listen()) {
            std::cerr << "Failed to listen on port " << port << " (see kfc_server.log)\n";
            return 1;
        }

        kfc::server::HttpApiServer http_server(http_port, users, rooms, sessions, metrics, auth_limiter, logger);
        if (!http_server.listen()) {
            std::cerr << "Failed to listen on HTTP port " << http_port << " (see kfc_server.log)\n";
            return 1;
        }

        // A signal handler can't safely call server.stop() itself (it takes
        // a mutex), so a small thread polls the flag instead.
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

        // Rooms must go quiet before the socket layer comes down, or a
        // frozen match's tick thread can broadcast into a closing socket.
        http_server.stop();
        rooms.stop_all();
    } catch (const std::exception& e) {
        logger.log(kfc::protocol::LogLevel::Error, std::string("Fatal: ") + e.what());
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
