#include "kfc/server/client_session.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "kfc/database/user_store.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/json.hpp"

namespace kfc::server {

ClientSession::ClientSession(std::string connection_id, SendFn send, CloseFn close, RoomManager& rooms,
                             kfc::database::IUserStore& users, SessionRegistry& sessions,
                             kfc::protocol::FileLogger& logger, Metrics* metrics, RateLimiter* auth_limiter,
                             std::string remote_ip, RateLimiter* seat_limiter)
    : connection_id_(std::move(connection_id)),
      send_(std::move(send)),
      close_(std::move(close)),
      rooms_(rooms),
      users_(users),
      sessions_(sessions),
      logger_(logger),
      metrics_(metrics),
      auth_limiter_(auth_limiter),
      remote_ip_(std::move(remote_ip)),
      seat_limiter_(seat_limiter) {}

void ClientSession::on_open() {
    logger_.log("Connection opened: " + connection_id_);
}

void ClientSession::on_close() {
    logger_.log("Connection closed: " + connection_id_);
    // A seated player who dropped forfeits; opponent is awarded the win.
    if (seat_.has_value()) {
        rooms_.on_disconnect(*seat_);
    }

    // Released now (not at destruction) so a player whose disconnect grace
    // is running can log in again immediately to reclaim their seat.
    username_lease_.reset();
}

void ClientSession::drop(const std::string& why) {
    logger_.log(kfc::protocol::LogLevel::Warning, "Dropping " + connection_id_ + ": " + why);
    close_();
}

void ClientSession::on_text(const std::string& text) {
    if (text.size() > kfc::protocol::kMaxMessageBytes) {
        if (metrics_ != nullptr) {
            metrics_->message_rejected();
        }
        drop("message of " + std::to_string(text.size()) + " bytes exceeds the limit");
        return;
    }

    auto now = std::chrono::steady_clock::now();
    if (now - rate_window_start_ >= std::chrono::seconds(1)) {
        rate_window_start_ = now;
        messages_in_window_ = 0;
    }
    if (++messages_in_window_ > kMaxMessagesPerSecond) {
        if (metrics_ != nullptr) {
            metrics_->message_rejected();
        }
        drop("more than " + std::to_string(kMaxMessagesPerSecond) + " messages in one second");
        return;
    }
    if (metrics_ != nullptr) {
        metrics_->message_received();
    }

    // Guarded: redact_for_log rebuilds the whole message, not worth doing
    // when this line will be dropped.
    if (logger_.enabled(kfc::protocol::LogLevel::Debug)) {
        logger_.log(kfc::protocol::LogLevel::Debug,
                    "Received from " + connection_id_ + ": " + kfc::protocol::redact_for_log(text));
    }

    std::optional<kfc::protocol::ClientMessage> decoded = kfc::protocol::decode_client_message(text);
    if (!decoded.has_value()) {
        if (metrics_ != nullptr) {
            metrics_->message_undecodable();
        }
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

    // Shares its budget with POST /api/auth/login and /register (see RateLimiter).
    if (auth_limiter_ != nullptr && !auth_limiter_->allow(remote_ip_, std::chrono::steady_clock::now())) {
        send_(kfc::protocol::encode(
            kfc::protocol::ServerMessage{kfc::protocol::LoginFailed{kfc::protocol::login_reasons::kRateLimited}}));
        drop(remote_ip_ + ": " + kfc::protocol::login_reasons::kRateLimited);
        return true;
    }

    const kfc::protocol::Login& login = std::get<kfc::protocol::Login>(message);
    kfc::database::IUserStore::AuthOutcome auth = users_.authenticate(login.username, login.password);
    if (!auth.ok) {
        send_(kfc::protocol::encode(kfc::protocol::ServerMessage{kfc::protocol::LoginFailed{auth.reason}}));
        drop("'" + login.username + "': " + auth.reason);
        return true;
    }

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
                                                        const AuthedUser& user, std::string& failure_reason,
                                                        std::string& redirect_url) {
    if (std::holds_alternative<kfc::protocol::Play>(message)) {
        return rooms_.join_any(user.username, user.rating, send_, close_);
    }
    if (std::holds_alternative<kfc::protocol::CreateRoom>(message)) {
        return rooms_.create_room(user.username, user.rating, send_, close_, &failure_reason);
    }

    const std::string& room = std::get<kfc::protocol::JoinRoom>(message).name;
    if (room.empty()) {
        failure_reason = kfc::protocol::join_reasons::kNoSuchRoom;
        return std::nullopt;
    }
    return rooms_.join_room(room, user.username, user.rating, send_, close_, &failure_reason, &redirect_url);
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

    // Separate budget from auth_limiter's -- see join_reasons::kRateLimited.
    if (seat_limiter_ != nullptr && !seat_limiter_->allow(remote_ip_, std::chrono::steady_clock::now())) {
        send_(kfc::protocol::encode(
            kfc::protocol::ServerMessage{kfc::protocol::JoinFailed{kfc::protocol::join_reasons::kRateLimited}}));
        drop(remote_ip_ + ": " + kfc::protocol::join_reasons::kRateLimited);
        return true;
    }

    std::string failure_reason;
    std::string redirect_url;
    std::optional<RoomManager::Seat> assigned = seat_for(message, *pending_, failure_reason, redirect_url);
    if (assigned.has_value()) {
        seat_ = assigned;
        return true;
    }

    if (!redirect_url.empty()) {
        send_(kfc::protocol::encode(kfc::protocol::ServerMessage{kfc::protocol::JoinRedirect{redirect_url}}));
        drop("'" + pending_->username + "': redirected to " + redirect_url);
        return true;
    }

    if (failure_reason.empty()) {
        failure_reason = kfc::protocol::join_reasons::kNoSuchRoom;
    }
    send_(kfc::protocol::encode(kfc::protocol::ServerMessage{kfc::protocol::JoinFailed{failure_reason}}));
    drop("'" + pending_->username + "': " + failure_reason);
    return true;
}

void ClientSession::handle_gameplay(const kfc::protocol::ClientMessage& message) {
    if (!seat_.has_value()) {
        logger_.log(kfc::protocol::LogLevel::Warning,
                    "Ignoring message from " + connection_id_ + ": not seated yet");
        return;
    }
    // Spectators are filtered here rather than in Match, which shouldn't need to know they exist.
    if (seat_->spectator) {
        logger_.log(kfc::protocol::LogLevel::Warning,
                    "Ignoring message from " + connection_id_ + ": watching, not playing");
        return;
    }
    if (metrics_ != nullptr &&
        (std::holds_alternative<kfc::protocol::MoveRequest>(message) ||
         std::holds_alternative<kfc::protocol::JumpRequest>(message))) {
        metrics_->move_processed();
    }
    rooms_.enqueue(seat_->room, seat_->color, message);
}

}  // namespace kfc::server
