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

// A thin adapter, not a second copy of the logic: the White/Black spelling
// itself lives in exactly one place, kfc::model::name_of (see piece_names.hpp
// -- it used to be re-derived here and in game_session.cpp too). This exists
// only because every call site below builds a std::string by concatenation,
// which std::string_view (name_of's return type) doesn't support directly.
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

std::optional<kfc::model::PieceColor> Match::join(const std::string& username, SendFn send, CloseFn close) {
    std::optional<kfc::model::PieceColor> assigned = audience_.seat(username, send, std::move(close));
    if (!assigned.has_value()) {
        return std::nullopt;  // both seats taken
    }

    logger_.log("Match: '" + username + "' joined as " + color_name(*assigned));
    send(kfc::protocol::encode(kfc::protocol::ServerMessage{welcome_for(*assigned, /*spectator=*/false)}));

    // Black is always the second (last) seat, so its join is the moment both
    // players are present -- tell both clients the match can begin. White, who
    // was "searching" since its own Welcome, transitions to play on this.
    if (*assigned == kfc::model::PieceColor::Black) {
        broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::MatchStart{}});
    }
    return assigned;
}

WatcherId Match::join_spectator(const std::string& username, SendFn send, CloseFn close) {
    logger_.log("Match: '" + username + "' is watching");

    // Registered *first*, snapshotted second. The other order loses updates:
    // a BoardUpdate broadcast between the snapshot and the registration reaches
    // nobody, and the viewer's board stays wrong for the rest of the game.
    //
    // This way nothing can be missed. The cost is the opposite risk -- an
    // update the snapshot already contains -- and that is what the revision is
    // for: the client discards any BoardUpdate at or below Welcome::revision.
    bool already_started = audience_.both_seats_taken();
    WatcherId watcher = audience_.watch(send, std::move(close));
    if (watcher == 0) {
        return 0;  // at MatchAudience::kMaxSpectators -- RoomManager sends JoinFailed
    }

    kfc::protocol::Welcome welcome = welcome_for(kfc::model::PieceColor::White, /*spectator=*/true);

    send(kfc::protocol::encode(kfc::protocol::ServerMessage{welcome}));
    // Sent to this one connection only: everyone else was told when it actually
    // happened.
    if (already_started) {
        send(kfc::protocol::encode(kfc::protocol::ServerMessage{kfc::protocol::MatchStart{}}));
    }
    return watcher;
}

void Match::leave_spectator(WatcherId watcher) {
    audience_.unwatch(watcher);
    logger_.log("Match: a viewer stopped watching");
}

kfc::protocol::Welcome Match::welcome_for(kfc::model::PieceColor color, bool spectator) const {
    // The board as it stands *right now*, not the starting position -- someone
    // walking in mid-game must see the game as it actually is. Snapshotted
    // under board_mutex_ because this runs on a connection thread, against a
    // board the tick thread may be mid-mutation of.
    kfc::protocol::Welcome welcome{color, {}, spectator, room_name_, {}, 0};
    std::lock_guard<std::mutex> board_guard(board_mutex_);
    welcome.board = kfc::protocol::snapshot_of(core_.board());
    // All three read under the one lock the tick thread also holds while it
    // changes them, so the board, the history and the revision always describe
    // the same instant.
    welcome.history = history_;
    welcome.revision = revision_;
    return welcome;
}

void Match::enqueue(kfc::model::PieceColor from, kfc::protocol::ClientMessage message) {
    {
        std::lock_guard<std::mutex> guard(queue_mutex_);
        if (queue_.size() >= kMaxQueuedCommands) {
            // Counted, not logged here: the whole point of the cap is that a
            // flood costs the server a fixed amount, and a log line per dropped
            // message would hand the denial of service straight back. The tick
            // thread reports the total once, after it drains.
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
    // Not a Resign -- the game only ends if the grace actually runs out. The
    // watch records it here and the tick loop, which wakes at least every
    // ~16ms, opens the countdown on its next advance().
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
        // One line per tick at worst, however hard the queue was flooded.
        logger_.log(kfc::protocol::LogLevel::Warning,
                    "Match: dropped " + std::to_string(dropped) + " command(s) -- queue full");
    }

    // All board-touching work for this tick runs under board_mutex_ so a
    // concurrent join() snapshot (on a connection thread) never reads the
    // board mid-mutation. Released before broadcasting below -- network
    // I/O must not hold the board lock. Applied in the order the queue
    // delivered them: deterministic, and the answer to "which of two
    // near-simultaneous commands wins" is whichever arrived first.
    //
    // engine().wait() itself -- the part that actually costs CPU -- only runs
    // while the match is genuinely Running. Not Frozen: a dropped player's
    // grace period stops the game clock as well as refusing their opponent's
    // commands (see apply); without this a piece already in flight would
    // still arrive, and every cooldown would still expire, while one side
    // sits at a disconnected client. Not Waiting: apply() refuses every
    // gameplay command until both seats are filled (kMatchNotStarted), so
    // nothing can be in flight to advance yet. Not Finished: once game_over_
    // latches, no new command can mutate the board (see apply), so further
    // calls buy nothing but CPU -- at the five-million-room scale
    // Server_Design.md targets, a room sitting in a matchmaking queue or
    // holding its post-game release delay is not rare, and ticking it exactly
    // like a live game 60 times a second for no observable effect is the
    // waste MatchScheduler was built to take seriously. Read once per tick,
    // before apply() runs below -- the very tick a resign or capture decides
    // the match, state() here still reports the state it was in when the
    // tick began, so that decisive tick still gets its engine().wait() call.
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
            // Kept so a client joining or returning mid-game can rebuild the
            // move list and score it never saw -- see welcome_for.
            history_.insert(history_.end(), events.begin(), events.end());
            if (!events.empty()) {
                // Bumped here, under the same lock as the board change, so
                // a snapshot taken concurrently can never report a revision
                // that disagrees with the position it contains.
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
            // Latch it so any command still in flight after the king fell
            // is dropped by apply() rather than mutating a decided board.
            game_over_ = true;
            std::optional<kfc::model::PieceColor> winner =
                game_over.is_draw() ? std::nullopt : game_over.winner();
            broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::GameOver{winner}});
            report_result(winner.has_value() ? GameEndReason::Decisive : GameEndReason::Draw, winner);
        }
    }

    // A resign ends the game without any arrival to ride along with, so its
    // GameOver is broadcast here, after board_mutex_ is released (network
    // I/O must not hold the board lock -- see above). A deliberate resign is
    // a decisive loss, so it feeds normal ELO (unlike a disconnect forfeit).
    if (pending_game_over_.has_value()) {
        broadcast_and_log(kfc::protocol::ServerMessage{*pending_game_over_});
        report_result(GameEndReason::Decisive, pending_game_over_->winner);
        pending_game_over_.reset();
    }

    advance_disconnect_countdown(now);

    // The match is decided -- by a capture, a resign, or a disconnect whose
    // grace ran out. Nobody is still playing here, so after a short grace
    // (so the GameOver just broadcast is seen first) everyone is let go.
    // Without this the survivor of a forfeit stays seated in a finished
    // game indefinitely, still sending moves at a board that will never
    // answer, and the room can never be reaped because a live connection
    // remains in it.
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
    // Closing a socket is network I/O, and each close comes back to this same
    // Match as an ordinary disconnect (which is what actually reaps the room).
    // That callback is delivered on the connection's own thread, never
    // synchronously on this one, so the reaping path's stop() never tries to
    // join the tick thread from inside itself.
    audience_.release_all();
}

void Match::advance_disconnect_countdown(std::chrono::steady_clock::time_point now) {
    // If the game already ended some other way (a capture during the grace, a
    // resign), stop counting anyone down and do nothing more.
    if (game_over_) {
        disconnect_watch_.clear();
        return;
    }

    DisconnectWatch::Tick tick = disconnect_watch_.advance(now);

    if (tick.expired_for.has_value()) {
        // Grace ran out -> forfeit the match for the player who never came back.
        kfc::model::PieceColor loser = *tick.expired_for;
        kfc::model::PieceColor winner = kfc::model::opposite_of(loser);
        game_over_ = true;
        logger_.log("Match: " + color_name(loser) + " did not return; " + color_name(winner) + " wins by forfeit");
        broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::GameOver{winner}});
        report_result(GameEndReason::Disconnect, winner);
        return;
    }

    // Set only when the displayed number actually changed, so this goes out
    // once a second rather than every tick.
    if (tick.seconds_remaining.has_value()) {
        broadcast_and_log(
            kfc::protocol::ServerMessage{kfc::protocol::OpponentDisconnected{*tick.seconds_remaining}});
    }
}

MatchState Match::state() const {
    // Ordered by precedence: a decided match is Finished whatever else is true,
    // and a frozen one is Frozen even though both seats are still filled.
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
    // Nothing to reclaim unless a countdown is actually running -- a player who
    // is still connected, or one whose grace already expired, owns no seat a
    // newcomer could step into.
    std::optional<kfc::model::PieceColor> counting_down = disconnect_watch_.watching();
    if (!counting_down.has_value() || game_over_) {
        return std::nullopt;
    }

    // ...and only for the very person who dropped. Anyone else arriving during
    // the countdown is just another joiner (i.e. a spectator), never a
    // substitute for the player whose game this is.
    if (audience_.username_of(*counting_down) != username) {
        return std::nullopt;
    }
    return counting_down;
}

bool Match::reconnect(kfc::model::PieceColor color, SendFn send, CloseFn close) {
    // Cancel the countdown first: until this lands, the tick thread is still one
    // tick away from forfeiting this very seat. cancel() returning false means
    // it already did -- this player is too late, and there is nothing to return
    // to. Reported rather than swallowed, so the caller can undo its own
    // bookkeeping (see RoomManager::join_room).
    if (game_over_ || !disconnect_watch_.cancel(color)) {
        logger_.log("Match: " + color_name(color) + " tried to return, but the grace had already expired");
        return false;
    }

    logger_.log("Match: " + color_name(color) + " reconnected");

    // Reseated *before* the snapshot, for the same reason join_spectator is --
    // an update broadcast in between would otherwise reach neither the dead
    // connection nor the new one, and the returning player would resume from a
    // position that never existed. Welcome::revision lets them discard the
    // updates the snapshot already covers.
    //
    // It also has to happen before any broadcast, so the OpponentReconnected
    // below reaches the returning player too.
    audience_.reseat(color, send, std::move(close));

    kfc::protocol::Welcome welcome = welcome_for(color, /*spectator=*/false);

    send(kfc::protocol::encode(kfc::protocol::ServerMessage{welcome}));
    send(kfc::protocol::encode(kfc::protocol::ServerMessage{kfc::protocol::MatchStart{}}));
    // Clears the countdown banner the opponent has been staring at.
    broadcast_and_log(kfc::protocol::ServerMessage{kfc::protocol::OpponentReconnected{}});
    return true;
}

bool Match::owns_piece_at(kfc::model::PieceColor from, const kfc::model::Position& cell) const {
    std::optional<kfc::model::Piece> piece = core_.board().piece_at(cell);
    // An empty/wrong cell is not an ownership violation -- GameEngine's own
    // "empty_source"/illegal-move rejection already covers that case with a
    // more specific reason; this check only ever fires kNotYourPiece for a
    // cell that does hold a piece, just not one belonging to `from`.
    return !piece.has_value() || piece->color == from;
}

void Match::apply(kfc::model::PieceColor from, const kfc::protocol::ClientMessage& message) {
    MatchState current = state();

    // Already decided (a king fell, or someone resigned/dropped): every later
    // command is a no-op. Nothing is sent back -- there is no board left to act
    // on and the result was already broadcast.
    if (current == MatchState::Finished) {
        return;
    }

    // Nobody to play against yet, so no gameplay command means anything --
    // including a resign: there is no opponent to award the win to, and the
    // result hook would be handed an empty username. Checked before the resign
    // below, not after. Without this gate the first player to be seated could
    // also move their pieces around while waiting to be matched, and their
    // opponent would walk into a game already in progress.
    if (current == MatchState::Waiting) {
        send_to_and_log(from, kfc::protocol::ServerMessage{
                                  kfc::protocol::MoveRejected{kfc::model::move_reasons::kMatchNotStarted}});
        return;
    }

    // A resign (also how a disconnect arrives -- see on_disconnect) ends the
    // game in the opponent's favour. It touches no board state, so it just
    // latches the outcome; tick_loop broadcasts the GameOver once board_mutex_
    // is released.
    //
    // Deliberately above the Frozen gate: giving up must stay possible while an
    // opponent's grace counts down. Everything else is refused there.
    if (std::holds_alternative<kfc::protocol::Resign>(message)) {
        kfc::model::PieceColor winner = kfc::model::opposite_of(from);
        logger_.log("Match: " + color_name(from) + " resigned; " + color_name(winner) + " wins");
        game_over_ = true;
        pending_game_over_ = kfc::protocol::GameOver{winner};
        return;
    }

    // A dropped player's grace period freezes the game -- see tick_loop, which
    // also stops the clock. Their opponent must not get to play on against
    // someone who cannot answer: twenty unopposed seconds is enough to walk a
    // piece up and take an undefended king, which would make the grace (and the
    // reconnect it exists for) worthless. Rejected rather than dropped, so the
    // client learns why instead of watching its click do nothing.
    if (current == MatchState::Frozen) {
        send_to_and_log(from, kfc::protocol::ServerMessage{
                                  kfc::protocol::MoveRejected{kfc::model::move_reasons::kOpponentDisconnected}});
        return;
    }

    // Captured before request_move/request_jump, but valid to read after
    // too -- Board only changes on arrival (see Motion's own doc comment),
    // so the piece is still sitting at this same cell either way. Used
    // only to look up the just-started Motion for MotionStarted below.
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
                // Login after the match is already joined is a no-op here --
                // join() is where a connection's first Login is handled,
                // before it ever reaches the queue.
                return kfc::model::MoveResult{true, "ok"};
            }
        },
        message);

    if (!result.is_accepted) {
        send_to_and_log(from, kfc::protocol::ServerMessage{kfc::protocol::MoveRejected{result.reason}});
        return;
    }

    // Broadcast the Motion RealTimeArbiter just started so both clients can
    // animate it locally (see MotionStarted's own doc comment) -- neither
    // GameEngine::request_move nor request_jump hands the Motion back
    // directly, so it's read back out of the arbiter by the id captured
    // above.
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
    // Debug: this is the message-by-message traffic, several a second per room.
    // Guarded so that concatenating the whole encoded message onto a prefix --
    // a second copy of it, per broadcast -- is skipped too when it is off, not
    // just the write.
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
