#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "kfc/engine/move_requester.hpp"
#include "kfc/events/event_bus.hpp"
#include "kfc/input/board_mapper.hpp"
#include "kfc/input/controller.hpp"
#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/messages.hpp"
#include "kfc/texttests/game_view.hpp"

namespace ix {
class WebSocket;
}

namespace kfc::graphics::net {

/// The networked counterpart to kfc::texttests::Game: implements IGameView
/// the same way, so MouseInputAdapter/PieceAnimatorRegistry never know the
/// difference, but every mutation is authoritative-server-driven instead of
/// locally simulated. Board is a pure mirror, replaced wholesale by Welcome
/// and updated only by replaying each BoardUpdate's ArrivalEvents -- there
/// is no local RealTimeArbiter here, on purpose (see the server plan): the
/// server alone decides what happened and when, and it alone ever writes
/// board_.
///
/// motion_for() *is* backed by real animation state, just not board_'s own:
/// the server broadcasts a MotionStarted the instant it starts a Move/Jump
/// (see match::Match::apply), and predicted_motions_ locally ticks each
/// one's elapsed_ms forward every wait() call purely so PieceAnimator has
/// something to interpolate against -- clamped to duration_ms so a slow
/// network never makes a piece overshoot its destination before the real
/// BoardUpdate confirms how the motion actually ended. That BoardUpdate
/// (never the prediction) is what actually moves pieces in board_; a
/// prediction is discarded the moment its piece appears in an arrival,
/// whether that arrival matches what was predicted or not (a rejected/
/// superseded motion is silently corrected the same way).
///
/// Threading: IXWebSocket delivers messages on its own background thread.
/// Rather than guard every board access with a mutex, incoming
/// BoardUpdate/MoveRejected/GameOver messages are queued (the one thing
/// that *is* mutex-protected) and only actually applied to board_ from
/// wait(), called once per frame by the render loop -- so every board
/// mutation happens on the same thread that reads it, same as Game's own
/// single-threaded assumption. Welcome is the one exception: it is awaited
/// synchronously by wait_for_welcome() before the render loop (and thus
/// any board access) ever starts, so no ongoing synchronization is needed
/// for board_/assigned_color_ after that point.
///
/// Internally owns a Controller (fed by *this as IMoveRequester) exactly
/// the way Game does, so click/jump selection logic is not duplicated.
class ServerLink : public kfc::texttests::IGameView, public kfc::model::IMoveRequester {
public:
    /// Connects asynchronously to server_url (e.g. "ws://localhost:8080")
    /// and sends a Login message once open. logger must outlive this
    /// ServerLink. Board access (board(), click(), jump()) is invalid until
    /// wait_for_welcome() returns true.
    /// seating_action is sent right after Login (once authenticated) to choose
    /// how to be seated: kfc::protocol::Play{} for matchmaking, or
    /// CreateRoom{name}/JoinRoom{name} for the named-room feature. The server
    /// only sends a Welcome after this.
    ServerLink(std::string server_url, std::string username, std::string password,
               kfc::protocol::ClientMessage seating_action, kfc::protocol::FileLogger& logger);
    ~ServerLink() override;

    ServerLink(const ServerLink&) = delete;
    ServerLink& operator=(const ServerLink&) = delete;

    /// Blocks (up to timeout_ms) for the server's Welcome response, which
    /// carries the assigned color and starting board, and builds the
    /// internal Board/Controller from it. Returns false on timeout --
    /// caller should treat that as a fatal startup error, the same way a
    /// missing board file already is in kfc_gui_app/main.cpp.
    [[nodiscard]] bool wait_for_welcome(int timeout_ms);

    /// Why the server refused to seat this connection, when wait_for_welcome()
    /// returned false: one of kfc::protocol::join_reasons (the room doesn't
    /// exist, its game is already over, the name is taken). std::nullopt when
    /// the failure was a plain timeout -- nothing answered at all, which is the
    /// "server unreachable" case rather than a refusal.
    ///
    /// A rejected *login* arrives here too, with kLoginFailurePrefix in front of
    /// the account store's own reason -- both failures share this one channel
    /// (neither can ever be followed by a Welcome), but they must not be worded
    /// alike to the player.
    [[nodiscard]] std::optional<std::string> join_failure() const;

    /// Marks a join_failure() that was really an authentication failure.
    static constexpr const char* kLoginFailurePrefix = "login:";

    /// Valid only after wait_for_welcome() returns true. Meaningless when
    /// is_spectator() -- a viewer is given no colour.
    [[nodiscard]] kfc::model::PieceColor assigned_color() const;

    /// True when the server seated this connection as a viewer rather than a
    /// player (it joined a named room whose two seats were already taken). The
    /// board mirrors the game exactly as a player's does; the difference is that
    /// nothing here may act on it -- click()/jump() do nothing, and the server
    /// would ignore the request anyway. Valid after wait_for_welcome().
    [[nodiscard]] bool is_spectator() const;

    /// The id of the room this connection ended up in, as the server reports it
    /// (Welcome::room) -- for Create it is the id the server just generated, and
    /// the only way this client can learn it. Empty for a Play (matchmaking)
    /// room, which has no id. Valid after wait_for_welcome().
    [[nodiscard]] const std::string& room_name() const;

    /// The arrivals this match produced before we joined (Welcome::history) --
    /// what a spectator walking in mid-game, or a player returning after a
    /// disconnect, needs to rebuild the move list and score it never saw. Empty
    /// when we were there from the start. Valid after wait_for_welcome().
    [[nodiscard]] const std::vector<kfc::model::ArrivalEvent>& history() const;

    /// True once the server has signalled both players are present (MatchStart).
    /// Between a successful Welcome and this, the client is "searching" -- seated
    /// in a room but waiting for a rating-compatible opponent to be matched in.
    [[nodiscard]] bool is_match_started() const;

    // --- IGameView ---
    kfc::input::ControllerResult click(int x, int y) override;
    kfc::input::ControllerResult jump(int x, int y) override;
    /// Drains the incoming-message queue and applies it to board_ -- see
    /// the class doc comment for why this, not on_message directly, is
    /// where board_ actually gets mutated. ms itself is unused: there is no
    /// local clock here, only "has anything arrived since last frame".
    void wait(int ms) override;
    /// The event bus each replayed ArrivalEvent is published on -- see
    /// IGameView::events. Subscribe before the render loop starts (the same
    /// single-threaded rule as Game): subscriptions are wired on the main
    /// thread up front, and publish() only ever runs later from wait(), also
    /// on the main thread.
    kfc::events::EventBus& events() override;
    const kfc::model::Board& board() const override;
    /// The locally-predicted Motion for piece_id, if the server has told us
    /// (via MotionStarted) that it started one and no arrival has resolved
    /// it yet -- see the class doc comment.
    std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const override;
    /// Always false -- legality/busy-checking is entirely the server's
    /// job now; this client never blocks a click on its own stale belief.
    bool is_piece_busy(kfc::model::PieceId piece_id) const override;

    // --- IMoveRequester ---
    /// Sends a Move message and returns optimistically (is_accepted=true)
    /// without waiting for the server's answer -- a genuine rejection
    /// arrives later as a MoveRejected message (logged; not yet surfaced to
    /// the UI -- no Phase 1 caller reads Controller's returned MoveResult
    /// for networked play, same as local play today).
    kfc::model::MoveResult request_move(const kfc::model::Position& source,
                                         const kfc::model::Position& destination) override;
    kfc::model::MoveResult request_jump(const kfc::model::Position& cell) override;

private:
    void on_message(const std::string& text);
    void apply_board_update(const kfc::protocol::BoardUpdate& update);
    void handle_motion_started(const kfc::protocol::MotionStarted& started);
    void send(const kfc::protocol::ClientMessage& message);

    std::string username_;
    std::string password_;
    kfc::protocol::ClientMessage seating_action_;
    kfc::protocol::FileLogger& logger_;
    std::unique_ptr<ix::WebSocket> socket_;

    // Welcome hand-off: written once on the IXWebSocket thread, awaited
    // synchronously by wait_for_welcome() on the main thread before
    // anything else touches board_ -- see the class doc comment.
    mutable std::mutex welcome_mutex_;
    std::condition_variable welcome_cv_;
    std::optional<kfc::protocol::Welcome> pending_welcome_;
    // The other way that same wait can end: the server refused to seat us and
    // said why. Guarded by welcome_mutex_ alongside pending_welcome_.
    std::optional<std::string> join_failure_;

    // main-thread-only from here on (see wait())
    std::optional<kfc::model::Board> board_;
    std::optional<kfc::model::PieceColor> assigned_color_;
    // Set from the Welcome: this connection watches rather than plays.
    bool spectator_ = false;
    // Also from the Welcome -- the room's server-assigned id, to display.
    std::string room_name_;
    // How far the game had got when our Welcome's board was snapshotted, and
    // then how far we have applied. Any BoardUpdate at or below this is already
    // reflected in the board we were handed -- see apply_board_update.
    std::uint64_t revision_ = 0;
    // And the arrivals that happened before we arrived. Replayed by the caller
    // into its own observers, not onto the bus: these already happened, so they
    // must not fire sounds or animations a second time.
    std::vector<kfc::model::ArrivalEvent> history_;
    std::optional<kfc::input::BoardMapper> board_mapper_;
    std::optional<kfc::input::Controller> controller_;
    // Re-derived every wait() from motion_start_times_ (an absolute anchor),
    // never accumulated frame-to-frame -- see that field's own comment for
    // why. Cleared per-piece the moment an arrival for that piece is
    // applied -- see the class doc comment.
    std::unordered_map<kfc::model::PieceId, kfc::model::Motion> predicted_motions_;
    // Wall-clock instant each predicted_motions_ entry's motion actually
    // started (steady_clock::now() when its MotionStarted arrived, minus
    // whatever elapsed_ms the server had already ticked it by). wait()
    // recomputes elapsed_ms as `now - this` every frame instead of adding
    // this frame's delta onto last frame's result -- an accumulation
    // approach silently loses time whenever a frame's real gap is clamped
    // down (e.g. this window losing focus/redraw priority to the other
    // client's window on the same machine), so the two clients' local
    // animations drift apart the longer either window goes unfocused.
    // Recomputing from an absolute anchor instead means a late frame simply
    // catches up in one jump, keeping both clients' predictions locked to
    // the same real time the server actually started the motion at.
    std::unordered_map<kfc::model::PieceId, std::chrono::steady_clock::time_point> motion_start_times_;

    std::mutex incoming_mutex_;
    std::vector<kfc::protocol::ServerMessage> incoming_queue_;

    // Wired on the main thread before the render loop, published to only from
    // wait() on that same thread -- so, like board_, it needs no lock (see the
    // class doc's threading note).
    kfc::events::EventBus events_;
    // Set when the server's MatchStart arrives (both players present); read by
    // is_match_started(). Written in wait() and read on the main thread, so no
    // synchronization is needed. Drives the one-shot GameStarted publish, so the
    // intro/sound play at real match start, not the moment we merely connect.
    bool match_started_ = false;
};

}  // namespace kfc::graphics::net
