#include "kfc/graphics/net/server_link.hpp"

#include <algorithm>
#include <chrono>

// IXWebSocket's Windows headers transitively pull in <windows.h>, whose
// min/max macros would otherwise swallow every std::min/std::max call below
// -- same guard kfc_gui_app/main.cpp already needs for its own <windows.h>
// include.
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
    : username_(std::move(username)),
      password_(std::move(password)),
      seating_action_(std::move(seating_action)),
      logger_(logger),
      socket_(std::make_unique<ix::WebSocket>()) {
    // Windows needs WSAStartup (what this wraps) called before any socket
    // use -- isolated here, not left to main(), so every current and future
    // caller of ServerLink gets it for free instead of having to remember
    // it separately, the same way MouseInputAdapter isolates OpenCV's
    // callback quirk. Safe to call from multiple ServerLink instances/
    // processes; each pairs with its own uninitNetSystem() in the
    // destructor.
    ix::initNetSystem();
    socket_->setUrl(server_url);
    socket_->setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) {
        if (msg->type == ix::WebSocketMessageType::Open) {
            logger_.log("ServerLink: connected, sending Login as '" + username_ + "'");
            send(kfc::protocol::ClientMessage{kfc::protocol::Login{username_, password_}});
            // Immediately follow with the seating choice (Play / Create / Join);
            // the server authenticates the Login, then seats us on this.
            send(seating_action_);
        } else if (msg->type == ix::WebSocketMessageType::Message) {
            on_message(msg->str);
        } else if (msg->type == ix::WebSocketMessageType::Close) {
            logger_.log("ServerLink: connection closed");
        } else if (msg->type == ix::WebSocketMessageType::Error) {
            logger_.log("ServerLink: connection error: " + msg->errorInfo.reason);
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
    logger_.log("ServerLink: sending " + encoded);
    socket_->send(encoded);
}

void ServerLink::on_message(const std::string& text) {
    logger_.log("ServerLink: received " + text);
    std::optional<kfc::protocol::ServerMessage> decoded = kfc::protocol::decode_server_message(text);
    if (!decoded.has_value()) {
        logger_.log("ServerLink: failed to decode message");
        return;
    }

    if (std::holds_alternative<kfc::protocol::Welcome>(*decoded)) {
        std::lock_guard<std::mutex> lock(welcome_mutex_);
        pending_welcome_ = std::get<kfc::protocol::Welcome>(*decoded);
        welcome_cv_.notify_all();
        return;
    }

    // The server refused to seat us and is about to hang up. Woken through the
    // same slot the Welcome uses, so wait_for_welcome() returns straight away
    // with a real reason instead of sitting out its whole timeout on a socket
    // that will never say anything else.
    if (std::holds_alternative<kfc::protocol::JoinFailed>(*decoded)) {
        std::lock_guard<std::mutex> lock(welcome_mutex_);
        join_failure_ = std::get<kfc::protocol::JoinFailed>(*decoded).reason;
        welcome_cv_.notify_all();
        return;
    }

    // Same slot, same reason: authentication failed, so there will never be a
    // Welcome. Prefixed so the caller can tell a rejected login apart from a
    // rejected room -- they need very different wording.
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
    std::unique_lock<std::mutex> lock(welcome_mutex_);
    bool arrived = welcome_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms), [this] {
        return pending_welcome_.has_value() || join_failure_.has_value();
    });
    // Either the server said no (join_failure_ holds why, for the caller to
    // show) or nothing came at all within the timeout.
    if (!arrived || join_failure_.has_value()) {
        return false;
    }

    const kfc::protocol::Welcome& welcome = *pending_welcome_;
    assigned_color_ = welcome.assigned_color;
    spectator_ = welcome.spectator;
    room_name_ = welcome.room;
    history_ = welcome.history;

    kfc::model::Board board(welcome.board.width, welcome.board.height);
    for (const kfc::model::Piece& piece : welcome.board.pieces) {
        board.add_piece(piece);
    }
    board_ = std::move(board);
    board_mapper_.emplace(board_->width(), board_->height());
    // Restrict click-selection to this client's own color: the opponent's
    // pieces can't be picked up locally, matching the server's own ownership
    // check (see Match::owns_piece_at).
    controller_.emplace(*board_, static_cast<kfc::model::IMoveRequester&>(*this), *board_mapper_, assigned_color_);

    pending_welcome_.reset();
    return true;
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
    // A viewer's clicks go nowhere. The caller (the GUI) already declines to
    // wire up mouse input for one, but a Controller built around a colour this
    // connection was never given must not be reachable even by accident.
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
                    // Re-published onto the local bus so the UI can render the
                    // countdown, same pattern as arrivals/GameEnded.
                    events_.publish(kfc::events::OpponentCountdown{m.seconds_remaining});
                } else if constexpr (std::is_same_v<T, kfc::protocol::OpponentReconnected>) {
                    // They made it back inside the grace -- clear the countdown
                    // banner and carry on.
                    events_.publish(kfc::events::OpponentReturned{});
                    logger_.log("ServerLink: opponent reconnected");
                } else if constexpr (std::is_same_v<T, kfc::protocol::JoinFailed>) {
                    // Only ever seen before wait_for_welcome() succeeds, where
                    // it is read off the pending slot instead (see on_message);
                    // logged here purely so a stray one is never silent.
                    logger_.log("ServerLink: join failed: " + m.reason);
                } else if constexpr (std::is_same_v<T, kfc::protocol::MatchStart>) {
                    // Both players present -> the match is really beginning.
                    // Publish GameStarted now (not on mere connection) so the
                    // intro splash and start sound land at the true start.
                    match_started_ = true;
                    events_.publish(kfc::events::GameStarted{});
                } else if constexpr (std::is_same_v<T, kfc::protocol::GameOver>) {
                    // The server declares game-over for every ending (king
                    // capture, resign, or disconnect), so publishing GameEnded
                    // here is the one hook that covers them all -- sound and the
                    // end banner both react to it, uniformly with local play.
                    events_.publish(kfc::events::GameEnded{m.winner});
                    logger_.log(m.winner.has_value() ? "ServerLink: game over, winner recorded"
                                                     : "ServerLink: game over, draw");
                }
                // Welcome never reaches this queue -- see on_message.
            },
            message);
    }

    // Re-derives every still-open prediction's elapsed_ms from its absolute
    // start time rather than accumulating this frame's delta onto the
    // last -- see motion_start_times_'s own comment for why (a frame's
    // clamped/delayed delta must never permanently lose real time from the
    // animation). Clamped to duration_ms: a slow/bursty network must never
    // let a predicted piece visually overshoot its destination and sit
    // there having "arrived" before the real BoardUpdate confirms it did.
    // ms itself is now unused here, but kept as the wait(int ms) override's
    // own parameter regardless (see IGameView).
    auto now = std::chrono::steady_clock::now();
    for (auto& [id, motion] : predicted_motions_) {
        int real_elapsed_ms = static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now - motion_start_times_.at(id)).count());
        motion.elapsed_ms = std::clamp(real_elapsed_ms, 0, motion.duration_ms);
    }
}

void ServerLink::handle_motion_started(const kfc::protocol::MotionStarted& started) {
    kfc::model::PieceId id = started.motion.moving_piece.id;
    predicted_motions_[id] = started.motion;
    // Anchored to when this motion actually began, not to when this client
    // happened to receive the broadcast -- subtracting the server's own
    // elapsed_ms (always 0 today, see MotionStarted's doc comment, but this
    // stays correct even if a future retry/replay ever delivers one
    // mid-flight) keeps this client's prediction aligned to the same
    // instant the server -- and thus the other client -- started counting
    // from.
    motion_start_times_[id] = std::chrono::steady_clock::now() - std::chrono::milliseconds(started.motion.elapsed_ms);
}

void ServerLink::apply_board_update(const kfc::protocol::BoardUpdate& update) {
    if (!board_.has_value()) {
        return;
    }

    for (const kfc::model::ArrivalEvent& event : update.arrival_events) {
        // Mirrors RealTimeArbiter::resolve_arrival's own net effect on Board
        // exactly, using only the publicly observable ArrivalEvent fields --
        // this Board never runs its own collision resolution, it only ever
        // replays what the server already decided.
        //
        // The destination cell is cleared *unconditionally*, not just on a
        // capture: the server also clears it when the mover passed through an
        // airborne enemy (no capture, but the enemy's stale record was still
        // sitting there). Clearing only on capture left that record in place,
        // so the add_piece below then hit an already-occupied cell and threw,
        // diverging the client's board from the server's. remove_piece is a
        // harmless no-op when the destination was genuinely empty, so this is
        // safe for an ordinary move too.
        board_->remove_piece(event.destination);
        board_->remove_piece(event.source);
        board_->add_piece(event.moved_piece);

        // The prediction (if any) for this arrival is now resolved --
        // whether it matched what was predicted or not, board_ has just
        // been given the real outcome, so nothing should keep animating
        // toward a guess anymore. A captured piece's own prediction (it may
        // have had one mid-flight, e.g. the race JumpRaceTest covers) is
        // cleared the same way -- it has no board presence left to animate.
        predicted_motions_.erase(event.moved_piece.id);
        motion_start_times_.erase(event.moved_piece.id);
        if (event.captured_piece.has_value()) {
            predicted_motions_.erase(event.captured_piece->id);
            motion_start_times_.erase(event.captured_piece->id);
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
    auto it = predicted_motions_.find(piece_id);
    if (it == predicted_motions_.end()) {
        return std::nullopt;
    }
    return it->second;
}

bool ServerLink::is_piece_busy(kfc::model::PieceId piece_id) const {
    // Doubles as PieceAnimatorRegistry's animator-retention signal (see its
    // own comment): a piece with an open prediction must keep its animator
    // alive even on a tick where board_ doesn't currently show it -- the
    // same transient "attacker provisionally occupies the defender's cell"
    // window local play already has to handle, now possible client-side
    // too since arrivals are replayed in the same order/batches the server
    // produced them.
    return predicted_motions_.count(piece_id) > 0;
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
