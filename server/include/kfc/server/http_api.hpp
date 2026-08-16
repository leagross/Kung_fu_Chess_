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
/// remote IP, on the *same* RateLimiter main() hands ClientSession's Login
/// path (see ClientSession::handle_login) -- both are ways to reach
/// UserRepository::authenticate(), and a shared budget is what stops an
/// attacker from simply moving from one to the other once the first is
/// throttled. This server does not own the limiter for that reason: it used
/// to (its own private instance, guarding only these two routes), and the
/// WebSocket path had nothing at all until it was given a reference to this
/// same one.
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
    /// users, rooms, sessions, metrics, auth_limiter and logger must all
    /// outlive this server. rooms and sessions back GET /metrics's two
    /// gauges (kfc_active_rooms, kfc_active_connections); metrics backs its
    /// counters -- see Metrics's own doc comment for why those two kinds of
    /// number come from different places. auth_limiter is the budget
    /// register/login share with the WebSocket Login path -- see this
    /// class's own doc comment. Brings up the network system and wires the
    /// request handler, but does not bind the port -- call listen() for
    /// that.
    HttpApiServer(int port, kfc::database::UserRepository& users, RoomManager& rooms, SessionRegistry& sessions,
                 Metrics& metrics, RateLimiter& auth_limiter, kfc::protocol::FileLogger& logger);
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
    // Not owned -- see the constructor's own doc comment on why this one
    // (unlike tokens_ above) is shared with ClientSession's Login path.
    RateLimiter& auth_limiter_;
    std::unique_ptr<ix::HttpServer> server_;
};

}  // namespace kfc::server
