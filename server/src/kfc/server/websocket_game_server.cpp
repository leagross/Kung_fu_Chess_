#include "kfc/server/websocket_game_server.hpp"

#include <optional>
#include <string>
#include <variant>

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include "kfc/model/piece.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/json.hpp"
#include "kfc/protocol/messages.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/database/user_repository.hpp"

namespace kfc::server {

namespace {

// A connection that has authenticated (Login) but not yet chosen how to be
// seated. Held until it sends Play / CreateRoom / JoinRoom, which turns it into
// an actual seat.
struct AuthedUser {
    std::string username;
    int rating;
};

}  // namespace

WebSocketGameServer::WebSocketGameServer(int port, RoomManager& rooms, kfc::database::UserRepository& users,
                                         kfc::protocol::FileLogger& logger)
    : port_(port), rooms_(rooms), users_(users), logger_(logger) {
    // Windows needs WSAStartup (what this wraps) before any socket use;
    // paired with uninitNetSystem() in the destructor.
    ix::initNetSystem();
    server_ = std::make_unique<ix::WebSocketServer>(port_, "0.0.0.0");

    server_->setOnConnectionCallback([this](std::weak_ptr<ix::WebSocket> weak_socket,
                                            std::shared_ptr<ix::ConnectionState> connection_state) {
        auto socket = weak_socket.lock();
        if (!socket) {
            return;
        }

        // Per-connection state, both shared_ptr so they outlive each callback
        // invocation. pending holds the authenticated user between Login and the
        // Play/CreateRoom/JoinRoom that seats them; seat is set once seated
        // (which room, which colour), after which Move/Jump/Resign are routed.
        auto pending = std::make_shared<std::optional<AuthedUser>>();
        auto seat = std::make_shared<std::optional<RoomManager::Seat>>();
        std::string connection_id = connection_state->getId();

        socket->setOnMessageCallback(
            [this, weak_socket, connection_id, pending, seat](const ix::WebSocketMessagePtr& msg) {
                if (msg->type == ix::WebSocketMessageType::Open) {
                    logger_.log("Connection opened: " + connection_id);
                    return;
                }
                if (msg->type == ix::WebSocketMessageType::Close) {
                    logger_.log("Connection closed: " + connection_id);
                    // A seated player who dropped forfeits: tell RoomManager so
                    // the opponent is awarded the win and the room is reaped once
                    // empty (see RoomManager::on_disconnect). A connection that
                    // closed before being seated was never a player.
                    if (seat->has_value()) {
                        rooms_.on_disconnect(**seat);
                    }
                    return;
                }
                if (msg->type != ix::WebSocketMessageType::Message) {
                    return;
                }

                // Redacted: a Login carries the password in clear.
                logger_.log("Received from " + connection_id + ": " +
                            kfc::protocol::redact_for_log(msg->str));
                std::optional<kfc::protocol::ClientMessage> decoded = kfc::protocol::decode_client_message(msg->str);
                if (!decoded.has_value()) {
                    logger_.log("Failed to decode message from " + connection_id);
                    return;
                }

                auto send = [weak_socket](const std::string& text) {
                    if (auto socket = weak_socket.lock()) {
                        socket->send(text);
                    }
                };
                // How the room lets this player go once its game is decided
                // (see Match::release_players). The close comes back through
                // the Close branch above as an ordinary disconnect, which is
                // what reaps the finished room.
                auto close_connection = [weak_socket]() {
                    if (auto socket = weak_socket.lock()) {
                        socket->close();
                    }
                };
                auto drop = [this, weak_socket, connection_id](const std::string& why) {
                    logger_.log("Dropping " + connection_id + ": " + why);
                    if (auto socket = weak_socket.lock()) {
                        socket->close();
                    }
                };

                // Login authenticates only -- seating waits for the client to
                // choose Play / Create / Join.
                if (std::holds_alternative<kfc::protocol::Login>(*decoded)) {
                    if (pending->has_value() || seat->has_value()) {
                        return;  // already logged in
                    }
                    const kfc::protocol::Login& login = std::get<kfc::protocol::Login>(*decoded);
                    kfc::database::UserRepository::AuthOutcome auth = users_.authenticate(login.username, login.password);
                    if (!auth.ok) {
                        // Say why before hanging up. Otherwise the client sees
                        // only a closed socket, times out, and reports whatever
                        // it was trying to do (Play/Create/Join) as the failure
                        // -- when the real problem was the password.
                        send(kfc::protocol::encode(
                            kfc::protocol::ServerMessage{kfc::protocol::LoginFailed{auth.reason}}));
                        drop("'" + login.username + "': " + auth.reason);
                        return;
                    }
                    *pending = AuthedUser{login.username, auth.rating};
                    logger_.log("Authenticated '" + login.username + "' (rating " + std::to_string(auth.rating) +
                                (auth.newly_registered ? ", new account)" : ")"));
                    return;
                }

                // Play / CreateRoom / JoinRoom seat an authenticated, not-yet-
                // seated connection.
                bool is_seating = std::holds_alternative<kfc::protocol::Play>(*decoded) ||
                                  std::holds_alternative<kfc::protocol::CreateRoom>(*decoded) ||
                                  std::holds_alternative<kfc::protocol::JoinRoom>(*decoded);
                if (is_seating) {
                    if (!pending->has_value()) {
                        logger_.log("Ignoring seating request from " + connection_id + ": not logged in");
                        return;
                    }
                    if (seat->has_value()) {
                        return;  // already seated
                    }
                    const AuthedUser& user = **pending;
                    std::optional<RoomManager::Seat> assigned;
                    // Filled by RoomManager whenever seating fails, so the
                    // client is told *which* thing went wrong.
                    std::string failure_reason;
                    if (std::holds_alternative<kfc::protocol::Play>(*decoded)) {
                        assigned = rooms_.join_any(user.username, user.rating, send, close_connection);
                    } else if (std::holds_alternative<kfc::protocol::CreateRoom>(*decoded)) {
                        // No name to validate: the server mints the id itself,
                        // so Create has no client input that could be wrong.
                        assigned = rooms_.create_room(user.username, send, close_connection, &failure_reason);
                    } else {
                        const std::string& room = std::get<kfc::protocol::JoinRoom>(*decoded).name;
                        if (room.empty()) {
                            failure_reason = kfc::protocol::join_reasons::kNoSuchRoom;
                        } else {
                            assigned = rooms_.join_room(room, user.username, send, close_connection,
                                                        &failure_reason);
                        }
                    }
                    if (assigned.has_value()) {
                        *seat = assigned;
                        return;
                    }
                    // Tell the client why *before* hanging up: a bare close
                    // leaves it guessing (and waiting out its Welcome timeout).
                    if (failure_reason.empty()) {
                        failure_reason = kfc::protocol::join_reasons::kNoSuchRoom;
                    }
                    send(kfc::protocol::encode(
                        kfc::protocol::ServerMessage{kfc::protocol::JoinFailed{failure_reason}}));
                    drop("'" + user.username + "': " + failure_reason);
                    return;
                }

                // Everything else (Move/Jump/Resign) needs a seat.
                if (!seat->has_value()) {
                    logger_.log("Ignoring message from " + connection_id + ": not seated yet");
                    return;
                }
                // A viewer owns no pieces, so it has nothing to move, jump or
                // resign. Dropped here rather than in Match, which should never
                // have to know non-players exist at all.
                if ((*seat)->spectator) {
                    logger_.log("Ignoring message from " + connection_id + ": watching, not playing");
                    return;
                }
                rooms_.enqueue((*seat)->room, (*seat)->color, *decoded);
            });
    });
}

WebSocketGameServer::~WebSocketGameServer() {
    if (server_) {
        server_->stop();
    }
    ix::uninitNetSystem();
}

bool WebSocketGameServer::listen() {
    std::pair<bool, std::string> result = server_->listen();
    if (!result.first) {
        logger_.log("Failed to listen on port " + std::to_string(port_) + ": " + result.second);
    }
    return result.first;
}

void WebSocketGameServer::start() {
    server_->start();
}

void WebSocketGameServer::wait() {
    server_->wait();
}

void WebSocketGameServer::stop() {
    server_->stop();
}

}  // namespace kfc::server
