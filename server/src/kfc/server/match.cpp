#include "kfc/server/match.hpp"

#include <chrono>
#include <string>
#include <utility>

#include "kfc/model/piece.hpp"
#include "kfc/model/piece_names.hpp"
#include "kfc/protocol/json.hpp"
#include "kfc/realtime/game_over_observer.hpp"
#include "kfc/rules/move_reasons.hpp"

namespace kfc::server {

namespace {

// Adapter: name_of returns a string_view, but call sites here concatenate.
std::string color_name(kfc::model::PieceColor color) {
    return std::string(kfc::model::name_of(color));
}

}  // namespace

Match::Match(kfc::model::Board board, kfc::protocol::FileLogger& logger, kfc::protocol::GameplayConfig config,
             ResultCallback on_result, int disconnect_grace_ms, int release_delay_ms, std::string room_name)
    : config_(std::move(config)),
      speed_provider_(config_),
      standard_policy_(config_.standard_cooldown_ms),
      jump_policy_(config_.jump_cooldown_ms),
      core_(std::move(board), standard_policy_, jump_policy_, speed_provider_, config_.meters_per_cell),
      logger_(logger),
      on_result_(std::move(on_result)),
      started_at_(std::chrono::system_clock::now()),
      release_delay_ms_(release_delay_ms),
      room_name_(std::move(room_name)),
      disconnect_watch_(disconnect_grace_ms) {}

Match::~Match() = default;

std::optional<kfc::model::PieceColor> Match::join(const std::string& username, int rating, SendFn send,
                                                   CloseFn close) {
    std::optional<kfc::model::PieceColor> assigned = audience_.seat(username, rating, send, std::move(close));
    if (!assigned.has_value()) {
        return std::nullopt;  // both seats taken
    }

    logger_.log("Match: '" + username + "' joined as " + color_name(*assigned));
    send(kfc::protocol::encode(kfc::protocol::ServerMessage{welcome_for(*assigned, /*spectator=*/false)}));

    // Black is always the second seat, so its join is when both players are present.
    if (*assigned == kfc::model::PieceColor::Black) {
        broadcast_and_log(kfc::protocol::ServerMessage{
            kfc::protocol::MatchStart{audience_.username_of(kfc::model::PieceColor::White), username,
                                      audience_.rating_of(kfc::model::PieceColor::White), rating}});
    }
    return assigned;
}

WatcherId Match::join_spectator(const std::string& username, SendFn send, CloseFn close) {
    logger_.log("Match: '" + username + "' is watching");

    // Registered before snapshotting: the other order can lose a BoardUpdate
    // broadcast in between. Welcome::revision lets the client discard any
    // duplicate the snapshot already covers.
    bool already_started = audience_.both_seats_taken();
    WatcherId watcher = audience_.watch(send, std::move(close));
    if (watcher == 0) {
        return 0;  // at MatchAudience::kMaxSpectators -- RoomManager sends JoinFailed
    }

    kfc::protocol::Welcome welcome = welcome_for(kfc::model::PieceColor::White, /*spectator=*/true);

    send(kfc::protocol::encode(kfc::protocol::ServerMessage{welcome}));
    if (already_started) {
        send(kfc::protocol::encode(kfc::protocol::ServerMessage{
            kfc::protocol::MatchStart{welcome.white_username, welcome.black_username, welcome.white_rating,
                                      welcome.black_rating}}));
    }
    return watcher;
}

void Match::leave_spectator(WatcherId watcher) {
    audience_.unwatch(watcher);
    logger_.log("Match: a viewer stopped watching");
}

kfc::protocol::Welcome Match::welcome_for(kfc::model::PieceColor color, bool spectator) const {
    // Snapshotted under board_mutex_: this runs on a connection thread against
    // a board the tick thread may be mid-mutation of.
    kfc::protocol::Welcome welcome{color, {}, spectator, room_name_, {}, 0};
    welcome.white_username = audience_.username_of(kfc::model::PieceColor::White);
    welcome.black_username = audience_.username_of(kfc::model::PieceColor::Black);
    welcome.white_rating = audience_.rating_of(kfc::model::PieceColor::White);
    welcome.black_rating = audience_.rating_of(kfc::model::PieceColor::Black);
    std::lock_guard<std::mutex> board_guard(board_mutex_);
    welcome.board = kfc::protocol::snapshot_of(core_.board());
    welcome.history = history_;
    welcome.revision = revision_;
    return welcome;
}

void Match::enqueue(kfc::model::PieceColor from, kfc::protocol::ClientMessage message) {
    {
        std::lock_guard<std::mutex> guard(queue_mutex_);
        if (queue_.size() >= kMaxQueuedCommands) {
            // Counted, not logged here: a log line per drop would itself be
            // the denial of service. tick() reports the total once, after draining.
            ++dropped_commands_;
            return;
        }
        queue_.emplace_back(from, std::move(message));
    }
    if (wake_hook_) {
        wake_hook_();
    }
}

void Match::set_wake_hook(std::function<void()> hook) {
    wake_hook_ = std::move(hook);
}

void Match::on_disconnect(kfc::model::PieceColor color) {
    logger_.log("Match: " + color_name(color) + " disconnected");
    disconnect_watch_.report_disconnect(color);
}

void Match::tick(std::chrono::steady_clock::time_point now, int elapsed_ms) {
    std::deque<std::pair<kfc::model::PieceColor, kfc::protocol::ClientMessage>> pending;
    int dropped = 0;
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        pending.swap(queue_);
        dropped = std::exchange(dropped_commands_, 0);
    }
    if (dropped > 0) {
        logger_.log(kfc::protocol::LogLevel::Warning,
                    "Match: dropped " + std::to_string(dropped) + " command(s) -- queue full");
    }

    // Board-touching work runs under board_mutex_ (released before
    // broadcasting -- network I/O must not hold it). engine().wait() only
    // runs while Running, read once before apply() so the deciding tick
    // still gets its wait() call.
    bool should_advance = state() == MatchState::Running;

    kfc::model::ArrivalEvents events;
    std::uint64_t at_revision = 0;
    {
        std::lock_guard<std::mutex> board_guard(board_mutex_);
        for (auto& [from, message] : pending) {
            apply(from, message);
        }

        if (should_advance) {
            events = core_.engine().wait(elapsed_ms);
            history_.insert(history_.end(), events.begin(), events.end());
            if (!events.empty()) {
                ++revision_;
            }
        }
        at_revision = revision_;
    }
    if (!events.empty()) {
        broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::BoardUpdate{events, at_revision}});

        kfc::model::GameOverObserver game_over;
        for (const auto& event : events) {
            game_over.on_arrival(event);
        }
        if (game_over.is_game_over()) {
            game_over_ = true;
            std::optional<kfc::model::PieceColor> winner =
                game_over.is_draw() ? std::nullopt : game_over.winner();
            broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::GameOver{winner}});
            report_result(winner.has_value() ? GameEndReason::Decisive : GameEndReason::Draw, winner);
        }
    }

    // A resign has no arrival to ride along with, so its GameOver is
    // broadcast here, after board_mutex_ is released.
    if (pending_game_over_.has_value()) {
        broadcast_and_log(kfc::protocol::ServerMessage{*pending_game_over_});
        report_result(GameEndReason::Decisive, pending_game_over_->winner);
        pending_game_over_.reset();
    }

    advance_disconnect_countdown(now);

    // Everyone is let go a short grace after the match is decided (so the
    // just-broadcast GameOver is seen first); otherwise the survivor of a
    // forfeit stays seated forever and the room can never be reaped.
    if (game_over_ && !release_at_.has_value()) {
        release_at_ = now + std::chrono::milliseconds(release_delay_ms_);
    }
    if (!released_ && release_at_.has_value() && now >= *release_at_) {
        release_participants();
    }
}

void Match::release_participants() {
    released_ = true;
    logger_.log("Match: releasing everyone -- the match is over");
    // Each close comes back as an ordinary disconnect, delivered on the
    // connection's own thread, which is what actually reaps the room.
    audience_.release_all();
}

void Match::advance_disconnect_countdown(std::chrono::steady_clock::time_point now) {
    if (game_over_) {
        disconnect_watch_.clear();
        return;
    }

    DisconnectWatch::Tick tick = disconnect_watch_.advance(now);

    if (tick.expired_for.has_value()) {
        kfc::model::PieceColor loser = *tick.expired_for;
        kfc::model::PieceColor winner = kfc::model::opposite_of(loser);
        game_over_ = true;
        logger_.log("Match: " + color_name(loser) + " did not return; " + color_name(winner) + " wins by forfeit");
        broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::GameOver{winner}});
        report_result(GameEndReason::Disconnect, winner);
        return;
    }

    if (tick.seconds_remaining.has_value()) {
        broadcast_and_log(
            kfc::protocol::ServerMessage{kfc::protocol::OpponentDisconnected{*tick.seconds_remaining}});
    }
}

MatchState Match::state() const {
    // Ordered by precedence: Finished beats Frozen beats Waiting.
    if (game_over_) {
        return MatchState::Finished;
    }
    if (disconnect_watch_.is_frozen()) {
        return MatchState::Frozen;
    }
    if (!audience_.both_seats_taken()) {
        return MatchState::Waiting;
    }
    return MatchState::Running;
}

bool Match::is_over() const {
    return game_over_;
}

std::optional<kfc::model::PieceColor> Match::reclaimable_seat_for(const std::string& username) const {
    std::optional<kfc::model::PieceColor> counting_down = disconnect_watch_.watching();
    if (!counting_down.has_value() || game_over_) {
        return std::nullopt;
    }

    // Only for the person who actually dropped; anyone else is a spectator.
    if (audience_.username_of(*counting_down) != username) {
        return std::nullopt;
    }
    return counting_down;
}

bool Match::reconnect(kfc::model::PieceColor color, SendFn send, CloseFn close) {
    // cancel() returning false means the grace already expired; caller must
    // undo its own bookkeeping (see RoomManager::join_room).
    if (game_over_ || !disconnect_watch_.cancel(color)) {
        logger_.log("Match: " + color_name(color) + " tried to return, but the grace had already expired");
        return false;
    }

    logger_.log("Match: " + color_name(color) + " reconnected");

    // Reseated before the snapshot (same reason as join_spectator) and before
    // any broadcast, so OpponentReconnected below reaches the returning player too.
    audience_.reseat(color, send, std::move(close));

    kfc::protocol::Welcome welcome = welcome_for(color, /*spectator=*/false);

    send(kfc::protocol::encode(kfc::protocol::ServerMessage{welcome}));
    send(kfc::protocol::encode(kfc::protocol::ServerMessage{
        kfc::protocol::MatchStart{welcome.white_username, welcome.black_username, welcome.white_rating,
                                  welcome.black_rating}}));
    // Clears the countdown banner the opponent has been staring at.
    broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::OpponentReconnected{}});
    return true;
}

bool Match::owns_piece_at(kfc::model::PieceColor from, const kfc::model::Position& cell) const {
    std::optional<kfc::model::Piece> piece = core_.board().piece_at(cell);
    // Empty cell is not an ownership violation; GameEngine's own
    // empty_source rejection already covers that case.
    return !piece.has_value() || piece->color == from;
}

void Match::apply(kfc::model::PieceColor from, const kfc::protocol::ClientMessage& message) {
    MatchState current = state();

    if (current == MatchState::Finished) {
        return;
    }

    // Checked before Resign below: with nobody to play against, there is no
    // opponent to award a resign's win to, and the first-seated player could
    // otherwise move pieces while still waiting to be matched.
    if (current == MatchState::Waiting) {
        send_to_and_log(from, kfc::protocol::ServerMessage{
                                  kfc::protocol::MoveRejected{kfc::model::move_reasons::kMatchNotStarted}});
        return;
    }

    // Deliberately above the Frozen gate: giving up must stay possible while
    // an opponent's grace counts down.
    if (std::holds_alternative<kfc::protocol::Resign>(message)) {
        kfc::model::PieceColor winner = kfc::model::opposite_of(from);
        logger_.log("Match: " + color_name(from) + " resigned; " + color_name(winner) + " wins");
        game_over_ = true;
        pending_game_over_ = kfc::protocol::GameOver{winner};
        return;
    }

    // Rejected rather than silently dropped so the client learns why.
    if (current == MatchState::Frozen) {
        send_to_and_log(from, kfc::protocol::ServerMessage{
                                  kfc::protocol::MoveRejected{kfc::model::move_reasons::kOpponentDisconnected}});
        return;
    }

    // Used to look up the just-started Motion for MotionStarted below.
    std::optional<kfc::model::PieceId> piece_id;

    kfc::model::MoveResult result = std::visit(
        [this, from, &piece_id](const auto& m) -> kfc::model::MoveResult {
            using T = std::decay_t<decltype(m)>;
            if constexpr (std::is_same_v<T, kfc::protocol::MoveRequest>) {
                if (!owns_piece_at(from, m.source)) {
                    return kfc::model::MoveResult{false, kfc::model::move_reasons::kNotYourPiece};
                }
                if (std::optional<kfc::model::Piece> piece = core_.board().piece_at(m.source); piece.has_value()) {
                    piece_id = piece->id;
                }
                return core_.engine().request_move(m.source, m.destination);
            } else if constexpr (std::is_same_v<T, kfc::protocol::JumpRequest>) {
                if (!owns_piece_at(from, m.cell)) {
                    return kfc::model::MoveResult{false, kfc::model::move_reasons::kNotYourPiece};
                }
                if (std::optional<kfc::model::Piece> piece = core_.board().piece_at(m.cell); piece.has_value()) {
                    piece_id = piece->id;
                }
                return core_.engine().request_jump(m.cell);
            } else {
                return kfc::model::MoveResult{true, "ok"};
            }
        },
        message);

    if (!result.is_accepted) {
        send_to_and_log(from, kfc::protocol::ServerMessage{kfc::protocol::MoveRejected{result.reason}});
        return;
    }

    // request_move/request_jump don't hand the Motion back directly, so it's
    // read back out of the arbiter by the id captured above.
    if (piece_id.has_value()) {
        if (std::optional<kfc::model::Motion> motion = core_.arbiter().motion_for(*piece_id); motion.has_value()) {
            broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::MotionStarted{*motion}});
        }
    }
}

void Match::report_result(GameEndReason reason, std::optional<kfc::model::PieceColor> winner) {
    if (result_reported_ || !on_result_) {
        return;
    }
    result_reported_ = true;
    on_result_(reason, winner, audience_.username_of(kfc::model::PieceColor::White),
               audience_.username_of(kfc::model::PieceColor::Black), started_at_);
}

void Match::broadcast_and_log(const kfc::protocol::ServerMessage& message) {
    std::string encoded = kfc::protocol::encode(message);
    // Guarded so the concatenation itself is skipped when debug logging is off.
    if (logger_.enabled(kfc::protocol::LogLevel::Debug)) {
        logger_.log(kfc::protocol::LogLevel::Debug, "Match: broadcasting " + encoded);
    }
    audience_.broadcast(encoded);
}

void Match::send_to_and_log(kfc::model::PieceColor color, const kfc::protocol::ServerMessage& message) {
    std::string encoded = kfc::protocol::encode(message);
    if (logger_.enabled(kfc::protocol::LogLevel::Debug)) {
        logger_.log(kfc::protocol::LogLevel::Debug, "Match: sending to " + color_name(color) + ": " + encoded);
    }
    audience_.send_to(color, encoded);
}

}  // namespace kfc::server
