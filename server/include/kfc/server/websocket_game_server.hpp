#pragma once

#include <memory>

namespace ix {
class WebSocketServer;
}

namespace kfc::protocol {
class FileLogger;
}

// The account store lives in its own layer (database/), not here -- the server
// uses it, never defines it.
namespace kfc::database {
class IUserStore;
}

namespace kfc::server {

class RoomManager;
class SessionRegistry;
class Metrics;
class RateLimiter;

/// Seconds between pings to an idle connection. IXWebSocket closes a
/// connection that misses a pong before the next interval, so this bounds a
/// dead connection to roughly twice its value; a real client answers pings
/// automatically and is unaffected.
///
/// Kept low also because IXWebSocketTransport::poll() blocks for up to a
/// full ping interval on a connection mid-close-handshake during shutdown,
/// overriding its own shorter close timeout -- a vendored library bound, not
/// fixable here, so a lower value keeps worst-case shutdown to a few seconds.
inline constexpr int kIdlePingIntervalSecs = 5;

/// ixwebsocket's own defaults (backlog 5, maxConnections 128) are sized for
/// a demo; a k6 load test stopped scaling at ~128 concurrent connections
/// nowhere near CPU/memory-bound. backlog is the OS TCP SYN queue depth;
/// maxConnections is ixwebsocket's accepted-but-not-yet-handed-off cap.
inline constexpr int kTcpBacklog = 1024;
inline constexpr std::size_t kMaxConnections = 100000;

/// Owns the WebSocket transport for one kfc_server: connection lifecycle,
/// turning the first Login into a room+colour via RoomManager::join_any,
/// routing decoded messages via RoomManager::enqueue, and reporting drops
/// via RoomManager::on_disconnect. Knows only how players talk to the
/// server; game rules and routing live in Match/RoomManager.
///
/// Authenticates username+password against users before the connection is
/// placed in a room.
class WebSocketGameServer {
public:
    /// rooms, users, sessions and logger must outlive this server. Does not
    /// bind the port -- call listen() for that.
    WebSocketGameServer(int port, RoomManager& rooms, kfc::database::IUserStore& users,
                        SessionRegistry& sessions, kfc::protocol::FileLogger& logger, Metrics* metrics = nullptr,
                        RateLimiter* auth_limiter = nullptr);
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
    std::unique_ptr<ix::WebSocketServer> server_;
};

}  // namespace kfc::server
