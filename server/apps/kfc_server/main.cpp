// The kfc_server executable: top-level startup only. It loads the board and
// the shared gameplay config, builds a RoomManager (which opens a fresh Match
// per room on demand), hands it to a WebSocketGameServer (the transport), and
// runs it. Everything about *how* players connect lives in WebSocketGameServer,
// *which game* a player is in lives in RoomManager, and *what the game does*
// lives in Match.
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "kfc/io/board_parser.hpp"
#include "kfc/model/board.hpp"
#include "kfc/model/piece.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/gameplay_config.hpp"
#include "kfc/database/rating_service.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/database/user_repository.hpp"
#include "kfc/server/websocket_game_server.hpp"

namespace {

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
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    kfc::protocol::FileLogger logger("kfc_server.log");
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
                                  const std::string& white, const std::string& black) {
            if (reason == kfc::server::GameEndReason::Disconnect) {
                // The winner is the opponent, so the loser (who dropped) is the
                // other colour -- dock them the flat forfeit penalty.
                const std::string& loser = winner == kfc::model::PieceColor::White ? black : white;
                kfc::database::apply_forfeit(users, loser);
            } else {
                kfc::database::apply_game_result(users, winner, white, black);
            }
        };

        kfc::server::RoomManager rooms(board_factory, logger, std::move(gameplay), on_result);

        kfc::server::WebSocketGameServer server(port, rooms, users, logger);
        if (!server.listen()) {
            std::cerr << "Failed to listen on port " << port << " (see kfc_server.log)\n";
            return 1;
        }

        server.start();
        std::cout << "kfc_server listening on ws://localhost:" << port << "\n";
        server.wait();
        // rooms' destructor stops every live room's tick thread as the scope
        // exits (after server, declared later, has stopped accepting).
    } catch (const std::exception& e) {
        logger.log(std::string("Fatal: ") + e.what());
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
