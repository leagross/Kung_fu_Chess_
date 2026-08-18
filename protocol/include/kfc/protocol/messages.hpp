#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

#include "kfc/model/piece.hpp"
#include "kfc/model/position.hpp"
#include "kfc/realtime/arrival_event.hpp"
#include "kfc/realtime/motion.hpp"

namespace kfc::protocol {

/// A flattened, ordered read of a Board's occupancy -- Board itself has no
/// such type (it owns cells_ privately and is queried by position), so this
/// is purely a wire/snapshot concern, kept in protocol rather than growing
/// kfc_core's public surface for a need only networking has.
struct BoardSnapshot {
    int width;
    int height;
    std::vector<kfc::model::Piece> pieces;
};

/// Sent once, right after a successful Login: which color the server
/// assigned this connection (first join = White, second = Black -- see
/// server::Match) and the board's starting position.
///
/// spectator marks a connection that joined a room whose two seats were already
/// taken (the spec's "the following people who join are viewers"): it receives
/// every broadcast exactly like a player, so its screen shows the same game, but
/// it owns no pieces and any command it sends is ignored. assigned_color is
/// meaningless for a spectator -- it is sent as White purely so the field is
/// always populated; a client must read spectator, never the colour, to decide
/// whether it may play.
struct Welcome {
    kfc::model::PieceColor assigned_color;
    BoardSnapshot board;
    bool spectator = false;
    /// The room's id, for the client to display across the top of the screen.
    /// Empty for a Play (matchmaking) room, which has no id anyone would type.
    /// Authoritative: for Create the client couldn't know it (the server minted
    /// it), and for Join it beats echoing back whatever the player typed.
    std::string room;
    /// Every arrival this match has already seen, oldest first. The board above
    /// says where the pieces are; this says how they got there, which is what a
    /// move list and a score are made of. Without it a spectator walking in
    /// mid-game -- or a player returning after a disconnect -- would show a
    /// correct board beside an empty move log and a score of 0-0. Empty for a
    /// match that has not started.
    std::vector<kfc::model::ArrivalEvent> history;
    /// How far the game had got when `board` was snapshotted -- see
    /// BoardUpdate::revision. The client remembers this and ignores any update
    /// at or below it, because the snapshot already reflects those arrivals.
    ///
    /// This is what makes joining a game in progress safe. A joiner is
    /// registered for broadcasts *before* its snapshot is taken, so it can
    /// never miss an update; the cost is that it may receive one the snapshot
    /// already contains, and the revision is how it tells.
    std::uint64_t revision = 0;
    /// Both seats' usernames, for the client to show whose game this is --
    /// the UI spec's "Presenting player names". Empty until that seat is
    /// actually filled (a spectator arriving between the first and second
    /// player sees black_username still empty). Optional on the wire (see
    /// decode_server_message) so an older client/server pair, or a
    /// hand-written Welcome, still decodes -- just without names to show.
    std::string white_username;
    std::string black_username;
};

// --- Client -> Server ---

/// A connection's opening message: who is logging in, and their password. The
/// server validates both against its account store (see server::UserRepository)
/// -- first login for a username registers it, later logins must match -- and
/// only then assigns a colour. password is validated server-side and never
/// stored client-side; over ws://localhost it travels in the clear, which is
/// the project's documented local-only posture (TLS is deliberately off).
struct Login {
    std::string username;
    std::string password;
};

struct MoveRequest {
    kfc::model::Position source;
    kfc::model::Position destination;
};

struct JumpRequest {
    kfc::model::Position cell;
};

/// Post-login "find me any opponent" (the Play button): the server seats the
/// sender into rating-based matchmaking (see RoomManager::join_any). Sent after
/// Login, once authenticated.
struct Play {};

/// Post-login "open a room for me" (the Room dialog's Create): the server makes
/// a new room, *generates* its id, and seats the sender as White. Carries no
/// name on purpose -- the spec's "Create: generated a new room id" means the id
/// is the server's to mint, not the client's to pick; the client learns it from
/// the Welcome that follows (Welcome::room) and shows it so the player can pass
/// it to whoever they arranged to play. This is also why Create can no longer
/// collide with an existing room.
struct CreateRoom {};

/// Post-login "join the room whose id is `name`" (the Room dialog's Join): the
/// server seats the sender into that existing room -- Black if it's their first
/// opponent, otherwise a spectator. name is the id the room's creator read off
/// their own screen. Fails (JoinFailed, then the connection is dropped) if no
/// such room, or if its game is already decided.
struct JoinRoom {
    std::string name;
};

/// The sender forfeits the game -- the server ends the match immediately and
/// awards the win to the opponent (see server::Match). Carries no fields: who
/// resigned is the connection's own assigned colour, which only the server
/// knows, never something a client gets to name. The server also raises the
/// exact same outcome for a player who simply disconnects (see
/// Match::on_disconnect), so a dropped connection and a deliberate resign end
/// the game identically.
struct Resign {};

/// Every message shape a client ever sends. A client connection is always
/// exactly one variant value on the wire at a time -- decode_client_message
/// is what turns wire JSON into one of these.
using ClientMessage = std::variant<Login, Play, CreateRoom, JoinRoom, MoveRequest, JumpRequest, Resign>;

// --- Server -> Client ---

/// Broadcast the instant the server actually starts a Move/JumpInPlace
/// motion (GameEngine::request_move/request_jump accepted it) -- not on
/// arrival, unlike BoardUpdate. Exists purely so a networked client can run
/// its own local prediction of the glide/jump animation between now and
/// whenever the real BoardUpdate confirms how it ended (see ServerLink);
/// the server's own Board is never mutated from this, only from arrivals.
struct MotionStarted {
    kfc::model::Motion motion;
};

/// Broadcast after any server tick that produced arrivals. Carries
/// ArrivalEvents verbatim -- the same type MoveLogObserver/ScoreObserver
/// already consume locally, so a networked client can observe exactly the
/// same events a local one would. Spelled out as std::vector<ArrivalEvent>
/// rather than the kfc::model::ArrivalEvents alias so this header only
/// needs arrival_event.hpp, not the much heavier real_time_arbiter.hpp
/// (Board/Motion/CollisionResolver) that alias happens to live in.
struct BoardUpdate {
    std::vector<kfc::model::ArrivalEvent> arrival_events;
    /// How far the game has got *after* applying these arrivals: a counter the
    /// server bumps once per update that produces any. Monotonic within a
    /// match, so a client can tell an update it has already accounted for from
    /// one it has not -- see Welcome::revision.
    std::uint64_t revision = 0;
};

/// Mirrors MoveResult::reason verbatim when a Move/JumpRequest was rejected
/// (e.g. "motion_in_progress", "game_over", an illegal-move reason copied
/// from RuleEngine) -- never sent for an accepted request, since acceptance
/// is silent: the client finds out what happened via the BoardUpdate that
/// eventually follows, same as GameEngine's own callers do today.
struct MoveRejected {
    std::string reason;
};

/// winner is std::nullopt for a draw (both kings captured at the exact same
/// simulated instant) -- mirrors GameOverObserver::winner()/is_draw()
/// exactly, see its own doc comment for what "same instant" means.
struct GameOver {
    std::optional<kfc::model::PieceColor> winner;
};

/// Broadcast to the still-connected player once per second while a dropped
/// opponent's grace period counts down: seconds_remaining goes 20, 19, ..., 1,
/// so their screen can show the countdown (see server::Match). If the opponent
/// doesn't return in time a GameOver follows; there is no separate "reconnected"
/// message, the countdown simply stops.
struct OpponentDisconnected {
    int seconds_remaining;
};

/// Broadcast the instant both seats of a room are filled -- i.e. a real
/// opponent is now present and play can begin. A player who joined first sits
/// on Welcome (seated, board known) but "searching" until this arrives; the
/// second player gets Welcome and this together. Lets the Play flow tell
/// "seated, waiting for an opponent" apart from "matched, game on".
/// Both players' usernames again, alongside the "both present" signal itself
/// -- the one player still needs to hear this, since White's own Welcome (the
/// only other carrier of these) was sent before Black existed to name. Black's
/// Welcome already had both, so this is redundant for Black, but broadcast is
/// one message to both seats, not two different ones -- see
/// Match::broadcast_and_log.
struct MatchStart {
    std::string white_username;
    std::string black_username;
};

/// Sent instead of Welcome when a seating request (Play / CreateRoom /
/// JoinRoom) could not be honoured, carrying *why* in a stable machine-readable
/// reason (see join_reasons below). The connection is closed right after, but
/// the client now learns what actually went wrong instead of inferring it from
/// a silent drop -- which it could only report as the catch-all "unreachable,
/// taken, full or missing", and only after waiting out its own Welcome timeout.
struct JoinFailed {
    std::string reason;
};

/// The reasons a JoinFailed can carry. Stable strings, same convention as
/// kfc::model::move_reasons: the client maps them to human text, so the wording
/// can change without touching the protocol.
namespace join_reasons {
/// JoinRoom named a room that does not exist (never created, or long finished
/// and torn down).
inline constexpr const char* kNoSuchRoom = "no_such_room";
/// The room exists but its game is already decided -- there is nothing live to
/// join or watch, so joining is refused rather than dropping the newcomer into
/// a finished board.
inline constexpr const char* kRoomNotActive = "room_not_active";
/// CreateRoom named a room that already exists.
inline constexpr const char* kRoomNameTaken = "room_name_taken";
/// The room already has as many spectators as it will take -- see
/// MatchAudience's own kMaxSpectators. Unlike the two player seats, this
/// exists purely as a resource cap: nothing about the game itself changes
/// with more viewers, but nothing stops one attacker from opening thousands
/// of watch connections to the same room otherwise, each one paid for on
/// every broadcast.
inline constexpr const char* kSpectatorLimitReached = "spectator_limit_reached";
/// Too many Play/CreateRoom/JoinRoom attempts from this connection's remote
/// IP in the current window -- see kfc::server::RateLimiter. A seating
/// attempt (this connection's one and only one) always closes the
/// connection whether it succeeds or fails, so reaching this budget takes a
/// fresh Login -- and therefore a fresh spend of login_reasons::kRateLimited's
/// own budget -- for every attempt; this exists as its own, separately
/// tunable budget rather than relying on that as an accident of how sessions
/// are torn down.
inline constexpr const char* kRateLimited = "rate_limited";
}  // namespace join_reasons

/// Sent instead of Welcome or JoinFailed when JoinRoom named a room that is
/// real but lives on a *different* kfc_server worker (see
/// server::RoomManager's room directory) -- not "no such room", but "not
/// here". url is that worker's own client-facing address; the client is
/// expected to reconnect there and resend Login and the seating request that
/// got redirected, the same way it would dial in fresh. The connection this
/// arrived on is closed right after, exactly like JoinFailed.
struct JoinRedirect {
    std::string url;
};

/// Sent when Login was rejected, carrying the account store's own reason (e.g.
/// "wrong_password"). The connection is closed right after. Without this a
/// failed login is indistinguishable, from the client's side, from an
/// unreachable server -- it would just see the socket close and time out, and
/// blame whatever it tried to do next.
struct LoginFailed {
    std::string reason;
};

/// The reasons a LoginFailed can carry, beyond the account store's own
/// ("wrong_password"). Same convention as join_reasons.
namespace login_reasons {
/// This username already has a live connection. One account, one session: two
/// at once would be one player in two games at the same time, and the returning
/// player's own seat could not be told apart from a second copy of them.
inline constexpr const char* kAlreadyLoggedIn = "already_logged_in";
/// Too many Login attempts from this connection's remote IP in the current
/// window -- see kfc::server::RateLimiter. Shared with POST /api/auth/login
/// and /register's own budget (see HttpApiServer), so splitting an attack
/// across the WebSocket and HTTP paths does not double it.
inline constexpr const char* kRateLimited = "rate_limited";
}  // namespace login_reasons

/// Broadcast when a player who dropped came back before their grace ran out --
/// the countdown the opponent's screen is showing must stop. There is no payload:
/// who returned is whoever the countdown was for, which only the server knows.
struct OpponentReconnected {};

/// Every message shape a client ever receives.
using ServerMessage = std::variant<Welcome, MotionStarted, BoardUpdate, MoveRejected, GameOver, OpponentDisconnected,
                                   MatchStart, JoinFailed, JoinRedirect, LoginFailed, OpponentReconnected>;

}  // namespace kfc::protocol
