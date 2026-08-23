#pragma once

#include <memory>

namespace ix {
class WebSocketServer;
}

namespace kfc::protocol {
class FileLogger;
}

// The account store lives in its own layer (database/), not here.
namespace kfc::database {
class IUserStore;
}

namespace kfc::server {

class RoomManager;
class SessionRegistry;
class Metrics;
class RateLimiter;

/// Bounds a dead connection to roughly twice this value; kept low also because poll() blocks
/// up to a full interval on a connection mid-close-handshake during shutdown.
inline constexpr int kIdlePingIntervalSecs = 5;

/// ixwebsocket's defaults (backlog 5, maxConnections 128) are sized for a demo; a k6 load
/// test stopped scaling at ~128 concurrent connections nowhere near CPU/memory-bound.
inline constexpr int kTcpBacklog = 1024;
inline constexpr std::size_t kMaxConnections = 100000;

/// Owns the WebSocket transport for one kfc_server. Game rules and routing live in Match/RoomManager.
class WebSocketGameServer {
public:
    /// rooms, users, sessions and logger must outlive this server. Does not bind the port -- call listen().
    WebSocketGameServer(int port, RoomManager& rooms, kfc::database::IUserStore& users,
                        SessionRegistry& sessions, kfc::protocol::FileLogger& logger, Metrics* metrics = nullptr,
                        RateLimiter* auth_limiter = nullptr, RateLimiter* seat_limiter = nullptr);
    ~WebSocketGameServer();

    WebSocketGameServer(const WebSocketGameServer&) = delete;
    WebSocketGameServer& operator=(const WebSocketGameServer&) = delete;

    /// Returns false (after logging the reason) if the port can't be listened on.
    [[nodiscard]] bool listen();
    /// Non-blocking: accepts connections on IXWebSocket's own background thread.
    void start();
    void wait();
    void stop();

private:
    int port_;
    RoomManager& rooms_;
    kfc::database::IUserStore& users_;
    SessionRegistry& sessions_;
    kfc::protocol::FileLogger& logger_;
    Metrics* metrics_;
    RateLimiter* auth_limiter_;
    RateLimiter* seat_limiter_;
    std::unique_ptr<ix::WebSocketServer> server_;
};

}  // namespace kfc::server
