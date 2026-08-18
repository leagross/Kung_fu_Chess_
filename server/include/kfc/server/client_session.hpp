#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "kfc/protocol/messages.hpp"
#include "kfc/server/connection_callbacks.hpp"
#include "kfc/server/metrics.hpp"
#include "kfc/server/rate_limiter.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/server/session_registry.hpp"

namespace kfc::protocol {
class FileLogger;
}

namespace kfc::database {
class IUserStore;
}

namespace kfc::server {

/// Max inbound WebSocket frames per connection per second before it is
/// dropped. A legitimate client cannot move faster than pieces come off
/// cooldown, so hitting this implies a hostile or looping client.
inline constexpr int kMaxMessagesPerSecond = 50;

/// One connected client: authenticating, being seated, and having its
/// gameplay messages routed to the right room.
///
/// A successful Login claims the username for this connection so one
/// account cannot be in two games at once (see SessionRegistry).
///
/// Order is fixed and enforced: Login, then exactly one of Play /
/// CreateRoom / JoinRoom, then Move / Jump / Resign. A refusal is always
/// sent to the client before the connection is closed.
///
/// Threading: IXWebSocket delivers one connection's callbacks on one thread,
/// in order, so the session's own state needs no lock. Everything it
/// reaches into is internally synchronized.
class ClientSession {
public:
    /// metrics and auth_limiter default to null; on_text skips the related
    /// work when null. auth_limiter, when given, shares its budget with the
    /// HTTP login/register endpoints (see RateLimiter).
    ClientSession(std::string connection_id, SendFn send, CloseFn close, RoomManager& rooms,
                  kfc::database::IUserStore& users, SessionRegistry& sessions,
                  kfc::protocol::FileLogger& logger, Metrics* metrics = nullptr,
                  RateLimiter* auth_limiter = nullptr, std::string remote_ip = {});

    void on_open();

    /// Forfeits the game if this client held a seat, and lets the room be
    /// reaped once nobody is left in it.
    void on_close();

    /// Refused without parsing if longer than kfc::protocol::kMaxMessageBytes;
    /// dropped if it does not decode.
    void on_text(const std::string& text);

    /// Where this client is seated, once it is. For tests and diagnostics.
    [[nodiscard]] const std::optional<RoomManager::Seat>& seat() const { return seat_; }

    /// Whether a Login has been accepted for this connection.
    [[nodiscard]] bool authenticated() const { return pending_.has_value() || seat_.has_value(); }

private:
    // A connection that has authenticated but not yet chosen how to be seated.
    // Held between Login and the Play / CreateRoom / JoinRoom that seats it.
    struct AuthedUser {
        std::string username;
        int rating = 0;
    };

    // Each returns true when it has dealt with the message, so on_text can stop.
    [[nodiscard]] bool handle_login(const kfc::protocol::ClientMessage& message);
    [[nodiscard]] bool handle_seating(const kfc::protocol::ClientMessage& message);
    void handle_gameplay(const kfc::protocol::ClientMessage& message);

    // redirect_url is set instead of failure_reason when the room exists but
    // lives on a different worker (JoinRoom only); caller sends JoinRedirect.
    [[nodiscard]] std::optional<RoomManager::Seat> seat_for(const kfc::protocol::ClientMessage& message,
                                                            const AuthedUser& user, std::string& failure_reason,
                                                            std::string& redirect_url);

    void drop(const std::string& why);

    std::string connection_id_;
    SendFn send_;
    CloseFn close_;
    RoomManager& rooms_;
    kfc::database::IUserStore& users_;
    SessionRegistry& sessions_;
    kfc::protocol::FileLogger& logger_;
    Metrics* metrics_;
    RateLimiter* auth_limiter_;
    std::string remote_ip_;

    std::optional<SessionRegistry::Lease> username_lease_;
    std::optional<AuthedUser> pending_;
    std::optional<RoomManager::Seat> seat_;

    // Current one-second rate-limit window; reset lazily on the first
    // message past its end (see on_text), not by a timer.
    std::chrono::steady_clock::time_point rate_window_start_;
    int messages_in_window_ = 0;
};

}  // namespace kfc::server
