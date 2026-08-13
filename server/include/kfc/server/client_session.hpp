#pragma once

#include <chrono>
#include <optional>
#include <string>

#include "kfc/protocol/messages.hpp"
#include "kfc/server/connection_callbacks.hpp"
#include "kfc/server/room_manager.hpp"
#include "kfc/server/session_registry.hpp"

namespace kfc::protocol {
class FileLogger;
}

namespace kfc::database {
class IUserStore;
}

namespace kfc::server {

/// The most inbound WebSocket frames one connection may send in any one
/// second before it is dropped. Generous on purpose, for the same reason as
/// kMaxQueuedCommands (see match.hpp): a legitimate client cannot usefully
/// move faster than pieces come off cooldown, so reaching even a fraction of
/// this means something is sending far faster than it can play -- most likely
/// a hostile or badly-looping client, not a real burst of catch-up traffic.
inline constexpr int kMaxMessagesPerSecond = 50;

/// One connected client, and everything that happens to it between connecting
/// and going away: authenticating, being seated, and having its gameplay
/// messages routed to the right room.
///
/// This used to live inside WebSocketGameServer's constructor, as a lambda
/// inside a lambda -- one function holding the whole protocol state machine
/// alongside the socket plumbing. Separating them is not only about length.
/// The session reaches its client through a SendFn and a CloseFn rather than
/// through a socket, which means **none of this needs IXWebSocket to run**:
/// the rules for what a client may do, and when, can be tested directly with
/// two lambdas, where before they could only be exercised by standing up a real
/// server and talking to it over a real port. WebSocketGameServer is left with
/// what it is actually for -- owning the transport and handing each connection
/// one of these.
///
/// A successful Login also claims the username for this connection, so one
/// account cannot be in two games at once -- see SessionRegistry, including
/// why that does not get in the way of a player reconnecting.
///
/// The order is fixed and each step is refused before the one before it has
/// happened: **Login**, then exactly one of Play / CreateRoom / JoinRoom, then
/// Move / Jump / Resign for as long as the game lasts. A refusal always says
/// why before the connection is closed -- a bare hang-up leaves a client to
/// wait out its own timeout and then guess at the reason.
///
/// Threading: IXWebSocket delivers one connection's callbacks on one thread, in
/// order, so the session's own state needs no lock. Everything it reaches into
/// -- RoomManager, UserRepository, FileLogger -- is internally synchronized,
/// because those *are* shared across connections.
class ClientSession {
public:
    /// connection_id only ever appears in the log, to tell connections apart.
    /// send and close are how this one client is reached and released; rooms,
    /// users, sessions and logger are shared and must outlive the session.
    ClientSession(std::string connection_id, SendFn send, CloseFn close, RoomManager& rooms,
                  kfc::database::IUserStore& users, SessionRegistry& sessions,
                  kfc::protocol::FileLogger& logger);

    /// The socket opened. Logging only -- nothing is decided until a Login
    /// arrives.
    void on_open();

    /// The socket closed. Forfeits the game if this client held a seat, and
    /// lets the room be reaped once nobody is left in it. A connection that
    /// closed before being seated was never a player and ends nothing.
    void on_close();

    /// One inbound frame of wire text. Refused without being parsed if it is
    /// longer than kfc::protocol::kMaxMessageBytes, and dropped if it does not
    /// decode -- see kfc::protocol::decode_client_message.
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

    // Asks RoomManager for a seat, whichever of the three ways was requested.
    // failure_reason is filled with a kfc::protocol::join_reasons string when
    // no seat comes back, so the client can be told which thing went wrong --
    // unless redirect_url ends up non-empty instead, which means the room is
    // real but lives on a different worker (JoinRoom only; see
    // RoomManager::join_room), and the caller should send a JoinRedirect
    // rather than a JoinFailed.
    [[nodiscard]] std::optional<RoomManager::Seat> seat_for(const kfc::protocol::ClientMessage& message,
                                                            const AuthedUser& user, std::string& failure_reason,
                                                            std::string& redirect_url);

    // Logs why, then closes the connection. The reason is always sent to the
    // client first, by the caller -- see the class comment.
    void drop(const std::string& why);

    std::string connection_id_;
    SendFn send_;
    CloseFn close_;
    RoomManager& rooms_;
    kfc::database::IUserStore& users_;
    SessionRegistry& sessions_;
    kfc::protocol::FileLogger& logger_;

    // This connection's hold on its username, taken once the password checks
    // out and given back when the session is destroyed -- see SessionRegistry.
    std::optional<SessionRegistry::Lease> username_lease_;
    std::optional<AuthedUser> pending_;
    std::optional<RoomManager::Seat> seat_;

    // Messages accepted so far in the current one-second window, and when
    // that window started. Reset lazily -- on the first message past its end
    // -- rather than by a timer; see on_text. Zero-initialized time_point is
    // the epoch, so the very first message always starts a fresh window.
    std::chrono::steady_clock::time_point rate_window_start_;
    int messages_in_window_ = 0;
};

}  // namespace kfc::server
