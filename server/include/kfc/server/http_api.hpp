#pragma once

#include <memory>
#include <string>
#include <unordered_set>

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

/// The non-realtime half of the server: register/login and match history over HTTP+JSON,
/// sharing the auth RateLimiter budget with ClientSession's WebSocket Login path.
class HttpApiServer {
public:
    /// Does not bind the port -- call listen() for that. trusted_proxies is the set of TCP peer
    /// addresses allowed to supply X-Forwarded-For (e.g. Caddy's address in the compose network);
    /// empty by default, meaning every request's rate-limit/log identity is its literal TCP peer.
    HttpApiServer(int port, kfc::database::UserRepository& users, RoomManager& rooms, SessionRegistry& sessions,
                 Metrics& metrics, RateLimiter& auth_limiter, kfc::protocol::FileLogger& logger,
                 std::unordered_set<std::string> trusted_proxies = {});
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
    std::unordered_set<std::string> trusted_proxies_;
    std::unique_ptr<ix::HttpServer> server_;
};

}  // namespace kfc::server
