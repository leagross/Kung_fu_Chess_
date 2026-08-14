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
/// match history, over plain HTTP+JSON rather than the WebSocket game
/// protocol. This is what used to be a separate Java/Spring Boot
/// "api-gateway" service with its own PostgreSQL -- folded in here instead,
/// against the same SQLite-backed UserRepository the WebSocket login flow
/// already uses, so there is exactly one account store, not two.
///
/// A second listening socket, not a second port on WebSocketGameServer's own
/// server: ix::HttpServer subclasses ix::WebSocketServer and cannot share a
/// port with another WebSocketServer instance. Lifecycle mirrors
/// WebSocketGameServer's exactly (listen/start/wait/stop) on purpose, so
/// main() wires the two up the same way.
///
/// Register and login each return a bearer token (see AuthTokenStore) along
/// with the username and rating; GET /api/history/{username} requires that
/// token in an `Authorization: Bearer <token>` header and refuses to serve
/// anyone else's history with it, even to another real account -- it used to
/// be open to anyone who knew or guessed a username.
///
/// POST /api/auth/register and /api/auth/login are also rate-limited per
/// remote IP (see RateLimiter) -- the WebSocket game protocol has had
/// per-connection message-rate limiting in ClientSession from early on;
/// these two endpoints had nothing until now, which made either one a bare
/// script away from a credential-stuffing or account-creation flood.
///
/// A brand-new username/password pair is also checked against
/// UserRepository's length and character rules before the account is
/// created; register reports which one failed (400, `{"reason": ...}`) so a
/// real client can show something the user can act on, rather than a bare
/// 409 that reads the same as "someone already has that name."
///
/// GET /metrics is the third thing this socket serves, alongside auth and
/// history: a Prometheus scrape target (see Metrics), unauthenticated like
/// /health -- both are operational surfaces meant for infrastructure, not a
/// player's own account.
class HttpApiServer {
public:
    /// users, rooms, sessions, metrics and logger must all outlive this
    /// server. rooms and sessions back GET /metrics's two gauges
    /// (kfc_active_rooms, kfc_active_connections); metrics backs its
    /// counters -- see Metrics's own doc comment for why those two kinds of
    /// number come from different places. Brings up the network system and
    /// wires the request handler, but does not bind the port -- call
    /// listen() for that.
    HttpApiServer(int port, kfc::database::UserRepository& users, RoomManager& rooms, SessionRegistry& sessions,
                 Metrics& metrics, kfc::protocol::FileLogger& logger);
    ~HttpApiServer();

    HttpApiServer(const HttpApiServer&) = delete;
    HttpApiServer& operator=(const HttpApiServer&) = delete;

    /// Binds the port. Returns false (after logging the reason) if it can't be
    /// listened on.
    [[nodiscard]] bool listen();
    /// Begins accepting connections on IXWebSocket's own background thread
    /// (non-blocking).
    void start();
    /// Blocks until the server is stopped.
    void wait();
    /// Stops accepting connections and closes the server socket.
    void stop();

private:
    int port_;
    kfc::database::UserRepository& users_;
    RoomManager& rooms_;
    SessionRegistry& sessions_;
    Metrics& metrics_;
    kfc::protocol::FileLogger& logger_;
    // Owned here, not injected: issuing and checking bearer tokens is this
    // layer's own implementation detail, not something main() or any other
    // caller needs to see or share.
    AuthTokenStore tokens_;
    // Same reasoning -- see RateLimiter's own doc comment for the numbers.
    RateLimiter login_and_register_limiter_;
    std::unique_ptr<ix::HttpServer> server_;
};

}  // namespace kfc::server
