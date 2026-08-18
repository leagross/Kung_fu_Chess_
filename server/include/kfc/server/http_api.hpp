#pragma once

#include <memory>

#include "kfc/server/auth_token_store.hpp"
#include "kfc/server/rate_limiter.hpp"

namespace ix {
class HttpServer;
}

namespace kfc::protocol {
class FileLogger;
}

namespace kfc::database {
class UserRepository;
}

namespace kfc::server {
class RoomManager;
class SessionRegistry;
class Metrics;
}  // namespace kfc::server

namespace kfc::server {

/// The non-realtime half of the server's public surface: register/login and
/// match history, over plain HTTP+JSON. Uses the same SQLite-backed
/// UserRepository the WebSocket login flow uses, so there is one account
/// store, not two.
///
/// A second listening socket, not a second port on WebSocketGameServer's own
/// server: ix::HttpServer cannot share a port with another WebSocketServer
/// instance. Lifecycle mirrors WebSocketGameServer's (listen/start/wait/stop).
///
/// Register and login return a bearer token (see AuthTokenStore);
/// GET /api/history/{username} requires that token and refuses to serve
/// anyone else's history with it.
///
/// POST /api/auth/register and /api/auth/login share a RateLimiter budget
/// with ClientSession's WebSocket Login path, so an attacker can't dodge a
/// throttle by switching between the two.
///
/// GET /metrics (Prometheus scrape target) is also served here, unauthenticated
/// like /health.
class HttpApiServer {
public:
    /// users, rooms, sessions, metrics, auth_limiter and logger must all
    /// outlive this server. Does not bind the port -- call listen() for that.
    HttpApiServer(int port, kfc::database::UserRepository& users, RoomManager& rooms, SessionRegistry& sessions,
                 Metrics& metrics, RateLimiter& auth_limiter, kfc::protocol::FileLogger& logger);
    ~HttpApiServer();

    HttpApiServer(const HttpApiServer&) = delete;
    HttpApiServer& operator=(const HttpApiServer&) = delete;

    /// Returns false (after logging the reason) if the port can't be listened on.
    [[nodiscard]] bool listen();
    /// Non-blocking: accepts connections on IXWebSocket's own background thread.
    void start();
    void wait();
    void stop();

private:
    int port_;
    kfc::database::UserRepository& users_;
    RoomManager& rooms_;
    SessionRegistry& sessions_;
    Metrics& metrics_;
    kfc::protocol::FileLogger& logger_;
    // Owned here: token issuing/checking is this layer's own implementation detail.
    AuthTokenStore tokens_;
    // Shared with ClientSession's Login path -- see class comment.
    RateLimiter& auth_limiter_;
    std::unique_ptr<ix::HttpServer> server_;
};

}  // namespace kfc::server
