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

/// How often an idle connection is pinged, in seconds. IXWebSocket closes a
/// connection with "Ping timeout" if a pong does not come back before the
/// *next* interval elapses (see IXWebSocketTransport::poll), so this bounds
/// an unresponsive or never-logged-in connection to roughly twice its value
/// before it is dropped -- a connection actually playing sends real traffic
/// far more often than this and is never affected by it.
inline constexpr int kIdlePingIntervalSecs = 30;

/// Owns the WebSocket transport for one kfc_server: the IXWebSocket network
/// system's lifetime, the server socket itself, and all per-connection
/// handling -- logging opens/closes, turning the first Login into a room+colour
/// via RoomManager::join_any, routing each decoded Move/Jump/Resign to that
/// room via RoomManager::enqueue, and reporting a drop via
/// RoomManager::on_disconnect. It knows only *how* players talk to the server;
/// it never touches game rules, the board, ownership, timing, or which game a
/// player belongs to -- routing lives in RoomManager, rules in Match. This is
/// the CTD SERVER split:
///   WebSocketGameServer -- how you connect
///   kfc::protocol       -- what language you speak
///   RoomManager / Match -- which game you're in / what the game does
///
/// On Login it authenticates the username+password against users (registering
/// a first-seen username, rejecting a wrong password) before the connection is
/// ever placed in a room -- so which credentials are valid is decided here,
/// while what those credentials *are* lives in UserRepository.
class WebSocketGameServer {
public:
    /// rooms, users, sessions and logger must outlive this server. Brings up the
    /// network system and wires the connection handler, but does not bind the
    /// port -- call listen() for that.
    WebSocketGameServer(int port, RoomManager& rooms, kfc::database::IUserStore& users,
                        SessionRegistry& sessions, kfc::protocol::FileLogger& logger);
    ~WebSocketGameServer();

    WebSocketGameServer(const WebSocketGameServer&) = delete;
    WebSocketGameServer& operator=(const WebSocketGameServer&) = delete;

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
    RoomManager& rooms_;
    kfc::database::IUserStore& users_;
    SessionRegistry& sessions_;
    kfc::protocol::FileLogger& logger_;
    std::unique_ptr<ix::WebSocketServer> server_;
};

}  // namespace kfc::server
