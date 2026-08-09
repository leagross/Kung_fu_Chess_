#pragma once

#include <memory>

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
class HttpApiServer {
public:
    /// users and logger must outlive this server. Brings up the network
    /// system and wires the request handler, but does not bind the port --
    /// call listen() for that.
    HttpApiServer(int port, kfc::database::UserRepository& users, kfc::protocol::FileLogger& logger);
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
    kfc::protocol::FileLogger& logger_;
    std::unique_ptr<ix::HttpServer> server_;
};

}  // namespace kfc::server
