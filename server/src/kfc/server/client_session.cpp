#include "kfc/server/client_session.hpp"

#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "kfc/database/user_repository.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/json.hpp"

namespace kfc::server {

ClientSession::ClientSession(std::string connection_id, SendFn send, CloseFn close, RoomManager& rooms,
                             kfc::database::UserRepository& users, SessionRegistry& sessions,
                             kfc::protocol::FileLogger& logger)
    : connection_id_(std::move(connection_id)),
      send_(std::move(send)),
      close_(std::move(close)),
      rooms_(rooms),
      users_(users),
      sessions_(sessions),
      logger_(logger) {}

void ClientSession::on_open() {
    logger_.log("Connection opened: " + connection_id_);
}

void ClientSession::on_close() {
    logger_.log("Connection closed: " + connection_id_);
    // A seated player who dropped forfeits: tell RoomManager so the opponent is
    // awarded the win and the room is reaped once empty (see
    // RoomManager::on_disconnect).
    if (seat_.has_value()) {
        rooms_.on_disconnect(*seat_);
    }

    // Released here, not merely when this object is destroyed: this is the same
    // moment the disconnect grace starts, so a player whose countdown is running
    // can always log in again to reclaim their seat. See SessionRegistry.
    username_lease_.reset();
}

void ClientSession::drop(const std::string& why) {
    logger_.log(kfc::protocol::LogLevel::Warning, "Dropping " + connection_id_ + ": " + why);
    close_();
}

void ClientSession::on_text(const std::string& text) {
    // Checked before anything else touches the frame -- before it is logged,
    // and before it is parsed. decode_client_message refuses an oversized text
    // too, but by then the bytes are already here; this is where the connection
    // sending them is hung up on, so it cannot keep doing it. Reported by
    // length, never by content: the content is what we declined to handle.
    if (text.size() > kfc::protocol::kMaxMessageBytes) {
        drop("message of " + std::to_string(text.size()) + " bytes exceeds the limit");
        return;
    }

    // Debug: every inbound frame, so guarded -- redact_for_log rebuilds the
    // whole message, which is not worth doing for a line that will be dropped.
    // Redacted because a Login carries the password in clear.
    if (logger_.enabled(kfc::protocol::LogLevel::Debug)) {
        logger_.log(kfc::protocol::LogLevel::Debug,
                    "Received from " + connection_id_ + ": " + kfc::protocol::redact_for_log(text));
    }

    std::optional<kfc::protocol::ClientMessage> decoded = kfc::protocol::decode_client_message(text);
    if (!decoded.has_value()) {
        logger_.log(kfc::protocol::LogLevel::Warning, "Failed to decode message from " + connection_id_);
        return;
    }

    if (handle_login(*decoded)) {
        return;
    }
    if (handle_seating(*decoded)) {
        return;
    }
    handle_gameplay(*decoded);
}

bool ClientSession::handle_login(const kfc::protocol::ClientMessage& message) {
    if (!std::holds_alternative<kfc::protocol::Login>(message)) {
        return false;
    }
    if (authenticated()) {
        return true;  // already logged in; a second Login changes nothing
    }

    const kfc::protocol::Login& login = std::get<kfc::protocol::Login>(message);
    kfc::database::UserRepository::AuthOutcome auth = users_.authenticate(login.username, login.password);
    if (!auth.ok) {
        // Say why before hanging up. Otherwise the client sees only a closed
        // socket, times out, and reports whatever it was trying to do
        // (Play/Create/Join) as the failure -- when the real problem was the
        // password.
        send_(kfc::protocol::encode(kfc::protocol::ServerMessage{kfc::protocol::LoginFailed{auth.reason}}));
        drop("'" + login.username + "': " + auth.reason);
        return true;
    }

    // Claimed only after the password checks out: an unauthenticated stranger
    // must not be able to lock a real account out by guessing at its name.
    std::optional<SessionRegistry::Lease> lease = sessions_.claim(login.username);
    if (!lease.has_value()) {
        send_(kfc::protocol::encode(kfc::protocol::ServerMessage{
            kfc::protocol::LoginFailed{kfc::protocol::login_reasons::kAlreadyLoggedIn}}));
        drop("'" + login.username + "': " + kfc::protocol::login_reasons::kAlreadyLoggedIn);
        return true;
    }

    username_lease_ = std::move(lease);
    pending_ = AuthedUser{login.username, auth.rating};
    logger_.log("Authenticated '" + login.username + "' (rating " + std::to_string(auth.rating) +
                (auth.newly_registered ? ", new account)" : ")"));
    return true;
}

std::optional<RoomManager::Seat> ClientSession::seat_for(const kfc::protocol::ClientMessage& message,
                                                        const AuthedUser& user, std::string& failure_reason) {
    if (std::holds_alternative<kfc::protocol::Play>(message)) {
        return rooms_.join_any(user.username, user.rating, send_, close_);
    }
    if (std::holds_alternative<kfc::protocol::CreateRoom>(message)) {
        // No name to validate: the server mints the id itself, so Create has no
        // client input that could be wrong.
        return rooms_.create_room(user.username, send_, close_, &failure_reason);
    }

    const std::string& room = std::get<kfc::protocol::JoinRoom>(message).name;
    if (room.empty()) {
        failure_reason = kfc::protocol::join_reasons::kNoSuchRoom;
        return std::nullopt;
    }
    return rooms_.join_room(room, user.username, send_, close_, &failure_reason);
}

bool ClientSession::handle_seating(const kfc::protocol::ClientMessage& message) {
    bool is_seating = std::holds_alternative<kfc::protocol::Play>(message) ||
                      std::holds_alternative<kfc::protocol::CreateRoom>(message) ||
                      std::holds_alternative<kfc::protocol::JoinRoom>(message);
    if (!is_seating) {
        return false;
    }
    if (!pending_.has_value()) {
        logger_.log(kfc::protocol::LogLevel::Warning,
                    "Ignoring seating request from " + connection_id_ + ": not logged in");
        return true;
    }
    if (seat_.has_value()) {
        return true;  // already seated
    }

    // Filled by RoomManager whenever seating fails, so the client is told
    // *which* thing went wrong.
    std::string failure_reason;
    std::optional<RoomManager::Seat> assigned = seat_for(message, *pending_, failure_reason);
    if (assigned.has_value()) {
        seat_ = assigned;
        return true;
    }

    // Tell the client why *before* hanging up: a bare close leaves it guessing
    // (and waiting out its Welcome timeout).
    if (failure_reason.empty()) {
        failure_reason = kfc::protocol::join_reasons::kNoSuchRoom;
    }
    send_(kfc::protocol::encode(kfc::protocol::ServerMessage{kfc::protocol::JoinFailed{failure_reason}}));
    drop("'" + pending_->username + "': " + failure_reason);
    return true;
}

void ClientSession::handle_gameplay(const kfc::protocol::ClientMessage& message) {
    // Everything else (Move/Jump/Resign) needs a seat.
    if (!seat_.has_value()) {
        logger_.log(kfc::protocol::LogLevel::Warning,
                    "Ignoring message from " + connection_id_ + ": not seated yet");
        return;
    }
    // A viewer owns no pieces, so it has nothing to move, jump or resign.
    // Dropped here rather than in Match, which should never have to know
    // non-players exist at all.
    if (seat_->spectator) {
        logger_.log(kfc::protocol::LogLevel::Warning,
                    "Ignoring message from " + connection_id_ + ": watching, not playing");
        return;
    }
    rooms_.enqueue(seat_->room, seat_->color, message);
}

}  // namespace kfc::server
