#include "kfc/graphics/net/server_link.hpp"

#include <algorithm>
#include <chrono>

// IXWebSocket's Windows headers pull in <windows.h>, whose min/max macros
// would otherwise swallow std::min/std::max below.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include "kfc/events/game_events.hpp"
#include "kfc/protocol/json.hpp"

namespace kfc::graphics::net {

ServerLink::ServerLink(std::string server_url, std::string username, std::string password,
                       kfc::protocol::ClientMessage seating_action, kfc::protocol::FileLogger& logger)
    : username_(std::move(username)), password_(std::move(password)), seating_action_(std::move(seating_action)),
      logger_(logger) {
    // Windows needs WSAStartup (what this wraps) before any socket use; safe
    // to call from multiple instances, each paired with uninitNetSystem().
    ix::initNetSystem();
    connect_to(server_url);
}

void ServerLink::connect_to(const std::string& url) {
    socket_ = std::make_unique<ix::WebSocket>();
    socket_->setUrl(url);
    socket_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            logger_.log("ServerLink: connected, sending Login as '" + username_ + "'");
            send(kfc::protocol::ClientMessage{kfc::protocol::Login{username_, password_}});
            send(seating_action_);
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            on_message(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            logger_.log("ServerLink: connection closed");
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            logger_.log(kfc::protocol::LogLevel::Error, "ServerLink: connection error: " + msg->errorInfo.reason);
        }
    });
    socket_->start();
}

ServerLink::~ServerLink() {
    if (socket_) {
        socket_->stop();
    }
    ix::uninitNetSystem();
}

void ServerLink::send(const kfc::protocol::ClientMessage& message) {
    std::string encoded = kfc::protocol::encode(message);
    // Redacted since the first message sent is a Login with a password.
    if (logger_.enabled(kfc::protocol::LogLevel::Debug)) {
        logger_.log(kfc::protocol::LogLevel::Debug,
                    "ServerLink: sending " + kfc::protocol::redact_for_log(encoded));
    }
    socket_->send(encoded);
}

void ServerLink::on_message(const std::string& text) {
    if (logger_.enabled(kfc::protocol::LogLevel::Debug)) {
        logger_.log(kfc::protocol::LogLevel::Debug,
                    "ServerLink: received " + kfc::protocol::redact_for_log(text));
    }
    std::optional<kfc::protocol::ServerMessage> decoded = kfc::protocol::decode_server_message(text);
    if (!decoded.has_value()) {
        logger_.log(kfc::protocol::LogLevel::Warning, "ServerLink: failed to decode message");
        return;
    }

    if (std::holds_alternative<kfc::protocol::Welcome>(*decoded)) {
        std::lock_guard<std::mutex> lock(welcome_mutex_);
        pending_welcome_ = std::get<kfc::protocol::Welcome>(*decoded);
        welcome_cv_.notify_all();
        return;
    }

    // Woken through the same slot Welcome uses, so wait_for_welcome() returns
    // immediately instead of sitting out its timeout.
    if (std::holds_alternative<kfc::protocol::JoinFailed>(*decoded)) {
        std::lock_guard<std::mutex> lock(welcome_mutex_);
        join_failure_ = std::get<kfc::protocol::JoinFailed>(*decoded).reason;
        welcome_cv_.notify_all();
        return;
    }

    // Only the flag is set here; reconnecting must happen from
    // wait_for_welcome() on the main thread, not this callback's thread
    // (which stopping this same socket would try to join).
    if (std::holds_alternative<kfc::protocol::JoinRedirect>(*decoded)) {
        std::lock_guard<std::mutex> lock(welcome_mutex_);
        pending_redirect_url_ = std::get<kfc::protocol::JoinRedirect>(*decoded).url;
        welcome_cv_.notify_all();
        return;
    }

    // Prefixed so the caller can tell a rejected login apart from a rejected
    // room -- they need different wording.
    if (std::holds_alternative<kfc::protocol::LoginFailed>(*decoded)) {
        std::lock_guard<std::mutex> lock(welcome_mutex_);
        join_failure_ = kLoginFailurePrefix + std::get<kfc::protocol::LoginFailed>(*decoded).reason;
        welcome_cv_.notify_all();
        return;
    }

    std::lock_guard<std::mutex> lock(incoming_mutex_);
    incoming_queue_.push_back(*decoded);
}

bool ServerLink::wait_for_welcome(int timeout_ms) {
    // At most two passes: the original attempt and one redirect retry. A
    // second redirect is treated as failure so a bad deployment can't loop.
    for (int attempt = 0; attempt < 2; ++attempt) {
        std::unique_lock<std::mutex> lock(welcome_mutex_);
        bool arrived = welcome_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
            return pending_welcome_.has_value() || join_failure_.has_value() || pending_redirect_url_.has_value();
        });
        if (!arrived) {
            return false;  // nothing answered at all -- "server unreachable"
        }

        if (pending_redirect_url_.has_value()) {
            if (attempt == 1) {
                return false;  // a second redirect in the same call -- see above
            }
            std::string redirect_url = *pending_redirect_url_;
            pending_redirect_url_.reset();
            // Must not hold welcome_mutex_ while stopping: it joins
            // IXWebSocket's thread, which on_message also locks this mutex on.
            lock.unlock();
            logger_.log("ServerLink: room is on another worker, reconnecting to " + redirect_url);
            socket_->stop();
            connect_to(redirect_url);
            continue;  // wait again, on the new connection
        }

        if (join_failure_.has_value()) {
            return false;
        }

        const kfc::protocol::Welcome& welcome = *pending_welcome_;
        assigned_color_ = welcome.assigned_color;
        spectator_ = welcome.spectator;
        room_name_ = welcome.room;
        history_ = welcome.history;
        revision_ = welcome.revision;

        kfc::model::Board board(welcome.board.width, welcome.board.height);
        for (const kfc::model::Piece& piece : welcome.board.pieces) {
            board.add_piece(piece);
        }
        board_ = std::move(board);
        board_mapper_.emplace(board_->width(), board_->height());
        // Restrict click-selection to this client's own color, matching the
        // server's ownership check.
        controller_.emplace(*board_, static_cast<kfc::model::IMoveRequester&>(*this), *board_mapper_,
                            assigned_color_);

        pending_welcome_.reset();
        return true;
    }
    return false;  // unreachable -- every path through the loop above returns
}

kfc::model::PieceColor ServerLink::assigned_color() const {
    return *assigned_color_;
}

std::optional<std::string> ServerLink::join_failure() const {
    std::lock_guard<std::mutex> lock(welcome_mutex_);
    return join_failure_;
}

bool ServerLink::is_spectator() const {
    return spectator_;
}

const std::string& ServerLink::room_name() const {
    return room_name_;
}

const std::vector<kfc::model::ArrivalEvent>& ServerLink::history() const {
    return history_;
}

bool ServerLink::is_match_started() const {
    return match_started_;
}

kfc::input::ControllerResult ServerLink::click(int x, int y) {
    if (spectator_) {
        return {kfc::input::ClickOutcome::Ignored, std::nullopt};
    }
    return controller_->click(x, y);
}

kfc::input::ControllerResult ServerLink::jump(int x, int y) {
    if (spectator_) {
        return {kfc::input::ClickOutcome::Ignored, std::nullopt};
    }
    return controller_->jump(x, y);
}

void ServerLink::wait(int ms) {
    std::vector<kfc::protocol::ServerMessage> pending;
    {
        std::lock_guard<std::mutex> lock(incoming_mutex_);
        pending.swap(incoming_queue_);
    }

    for (const kfc::protocol::ServerMessage& message : pending) {
        std::visit(
            [this](const auto& m) {
                using T = std::decay_t<decltype(m)>;
                if constexpr (std::is_same_v<T, kfc::protocol::MotionStarted>) {
                    handle_motion_started(m);
                } else if constexpr (std::is_same_v<T, kfc::protocol::BoardUpdate>) {
                    apply_board_update(m);
                } else if constexpr (std::is_same_v<T, kfc::protocol::MoveRejected>) {
                    logger_.log("ServerLink: move rejected: " + m.reason);
                } else if constexpr (std::is_same_v<T, kfc::protocol::OpponentDisconnected>) {
                    events_.publish(kfc::events::OpponentCountdown{m.seconds_remaining});
                } else if constexpr (std::is_same_v<T, kfc::protocol::OpponentReconnected>) {
                    events_.publish(kfc::events::OpponentReturned{});
                    logger_.log("ServerLink: opponent reconnected");
                } else if constexpr (std::is_same_v<T, kfc::protocol::JoinFailed>) {
                    logger_.log("ServerLink: join failed: " + m.reason);
                } else if constexpr (std::is_same_v<T, kfc::protocol::MatchStart>) {
                    // Publish now, not on mere connection, so the intro splash
                    // and start sound land at the true match start.
                    match_started_ = true;
                    events_.publish(kfc::events::GameStarted{});
                } else if constexpr (std::is_same_v<T, kfc::protocol::GameOver>) {
                    events_.publish(kfc::events::GameEnded{m.winner});
                    logger_.log(m.winner.has_value() ? "ServerLink: game over, winner recorded"
                                                     : "ServerLink: game over, draw");
                }
            },
            message);
    }

    motion_predictor_.tick(std::chrono::steady_clock::now());
}

void ServerLink::handle_motion_started(const kfc::protocol::MotionStarted& started) {
    // Anchored to when the motion actually began (subtracting the server's
    // own elapsed_ms), so this client's prediction stays aligned with the
    // instant the server -- and the other client -- started counting from.
    auto started_at = std::chrono::steady_clock::now() - std::chrono::milliseconds(started.motion.elapsed_ms);
    motion_predictor_.start(started.motion, started_at);
}

void ServerLink::apply_board_update(const kfc::protocol::BoardUpdate& update) {
    if (!board_.has_value()) {
        return;
    }

    // The server snapshots its board after registering us for broadcasts, so
    // the first update or two may already be reflected in the snapshot;
    // replaying one would move a piece twice and diverge from the server.
    if (update.revision != 0 && update.revision <= revision_) {
        logger_.log(kfc::protocol::LogLevel::Debug,
                    "ServerLink: skipping update " + std::to_string(update.revision) +
                        ", already covered by the snapshot at " + std::to_string(revision_));
        return;
    }
    revision_ = update.revision;

    for (const kfc::model::ArrivalEvent& event : update.arrival_events) {
        // Destination is cleared unconditionally, not just on capture: the
        // server also clears it when the mover passed through an airborne
        // enemy with no capture, whose stale record would otherwise make
        // add_piece below throw on an "occupied" cell.
        board_->remove_piece(event.destination);
        board_->remove_piece(event.source);
        board_->add_piece(event.moved_piece);

        // The real outcome has arrived, so any prediction for this piece
        // (or a captured piece caught mid-flight) is now stale.
        motion_predictor_.discard(event.moved_piece.id);
        if (event.captured_piece.has_value()) {
            motion_predictor_.discard(event.captured_piece->id);
        }
    }

    for (const kfc::model::ArrivalEvent& event : update.arrival_events) {
        events_.publish(event);
    }
}

kfc::events::EventBus& ServerLink::events() {
    return events_;
}

const kfc::model::Board& ServerLink::board() const {
    return *board_;
}

std::optional<kfc::model::Motion> ServerLink::motion_for(kfc::model::PieceId piece_id) const {
    return motion_predictor_.motion_for(piece_id);
}

bool ServerLink::is_piece_busy(kfc::model::PieceId piece_id) const {
    // Doubles as PieceAnimatorRegistry's animator-retention signal: a piece
    // with an open prediction must keep its animator alive even when board_
    // doesn't currently show it.
    return motion_predictor_.is_tracked(piece_id);
}

kfc::model::MoveResult ServerLink::request_move(const kfc::model::Position& source,
                                                 const kfc::model::Position& destination) {
    send(kfc::protocol::ClientMessage{kfc::protocol::MoveRequest{source, destination}});
    return kfc::model::MoveResult{true, "ok"};
}

kfc::model::MoveResult ServerLink::request_jump(const kfc::model::Position& cell) {
    send(kfc::protocol::ClientMessage{kfc::protocol::JumpRequest{cell}});
    return kfc::model::MoveResult{true, "ok"};
}

}  // namespace kfc::graphics::net
