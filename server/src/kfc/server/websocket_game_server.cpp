#include "kfc/server/websocket_game_server.hpp"

#include <memory>
#include <string>

#include <ixwebsocket/IXConnectionState.h>
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>
#include <ixwebsocket/IXWebSocketServer.h>

#include "kfc/protocol/file_logger.hpp"
#include "kfc/server/client_session.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/server/session_registry.hpp"

namespace kfc::server {

WebSocketGameServer::WebSocketGameServer(int port, RoomManager& rooms, kfc::database::IUserStore& users,
                                         SessionRegistry& sessions, kfc::protocol::FileLogger& logger,
                                         Metrics* metrics, RateLimiter* auth_limiter, RateLimiter* seat_limiter)
    : port_(port),
      rooms_(rooms),
      users_(users),
      sessions_(sessions),
      logger_(logger),
      metrics_(metrics),
      auth_limiter_(auth_limiter),
      seat_limiter_(seat_limiter) {
    // Windows needs WSAStartup (what this wraps) before any socket use;
    // paired with uninitNetSystem() in the destructor.
    ix::initNetSystem();
    server_ = std::make_unique<ix::WebSocketServer>(port_, "0.0.0.0", kTcpBacklog, kMaxConnections);

    server_->setOnConnectionCallback([this](std::weak_ptr<ix::WebSocket> weak_socket,
                                            std::shared_ptr<ix::ConnectionState> connection_state) {
        auto socket = weak_socket.lock();
        if (!socket) {
            return;
        }

        // Off by default in IXWebSocket; ensures a connection that sends
        // nothing (never logs in, hangs mid-handshake) is eventually closed.
        socket->setPingInterval(kIdlePingIntervalSecs);

        // weak, not shared: send may be called long after this connection
        // went away (a room broadcasting to a dropped player), and a strong
        // reference would keep the dead socket alive.
        auto send = [weak_socket](const std::string& text) {
            if (auto live = weak_socket.lock()) {
                live->send(text);
            }
        };
        // The close comes back through the Close branch below as an
        // ordinary disconnect, reaping the finished room.
        auto close_connection = [weak_socket]() {
            if (auto live = weak_socket.lock()) {
                live->close();
            }
        };

        auto session = std::make_shared<ClientSession>(connection_state->getId(), send, close_connection, rooms_,
                                                       users_, sessions_, logger_, metrics_, auth_limiter_,
                                                       connection_state->getRemoteIp(), seat_limiter_);

        socket->setOnMessageCallback([session](const ix::WebSocketMessagePtr& msg) {
            switch (msg->type) {
                case ix::WebSocketMessageType::Open:
                    session->on_open();
                    return;
                case ix::WebSocketMessageType::Close:
                    session->on_close();
                    return;
                case ix::WebSocketMessageType::Message:
                    session->on_text(msg->str);
                    return;
                default:
                    return;  // Ping/Pong/Error/Fragment: nothing for us to do
            }
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
        logger_.log(kfc::protocol::LogLevel::Error,
                    "Failed to listen on port " + std::to_string(port_) + ": " + result.second);
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
