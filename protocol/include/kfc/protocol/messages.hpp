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

/// A flattened, ordered read of a Board's occupancy -- a wire/snapshot
/// concern only, kept out of Board's own public surface.
struct BoardSnapshot {
    int width;
    int height;
    std::vector<kfc::model::Piece> pieces;
};

/// Sent once, right after a successful Login: which color the server
/// assigned this connection (first join = White, second = Black) and the
/// board's starting position.
///
/// spectator marks a connection that joined a room whose two seats were
/// already taken: it receives every broadcast like a player but owns no
/// pieces and any command it sends is ignored. assigned_color is meaningless
/// for a spectator (sent as White so the field stays populated) -- a client
/// must read spectator, never the colour, to decide whether it may play.
struct Welcome {
    kfc::model::PieceColor assigned_color;
    BoardSnapshot board;
    bool spectator = false;
    /// The room's id, for the client to display. Empty for a Play
    /// (matchmaking) room. Authoritative even for Join, where it beats
    /// echoing back whatever the player typed.
    std::string room;
    /// Every arrival this match has already seen, oldest first -- lets a
    /// spectator or reconnecting player see the move list/score instead of a
    /// board with no history. Empty for a match that has not started.
    std::vector<kfc::model::ArrivalEvent> history;
    /// How far the game had got when `board` was snapshotted -- see
    /// BoardUpdate::revision. The client ignores any update at or below this,
    /// since the snapshot already reflects those arrivals. A joiner is
    /// registered for broadcasts before its snapshot is taken, so it can
    /// receive an update the snapshot already contains but never miss one.
    std::uint64_t revision = 0;
};

// --- Client -> Server ---

/// A connection's opening message: who is logging in, and their password.
/// First login for a username registers it; later logins must match. Travels
/// in the clear over ws://localhost (TLS is deliberately off, local-only).
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

/// Post-login "find me any opponent" (the Play button): seats the sender into
/// rating-based matchmaking.
struct Play {};

/// Post-login "open a room for me" (Create): the server makes a new room,
/// generates its id, and seats the sender as White. Carries no name -- the id
/// is the server's to mint; the client learns it from Welcome::room.
struct CreateRoom {};

/// Post-login "join the room whose id is `name`" (Join): seats the sender
/// into that room -- Black if it's their first opponent, otherwise a
/// spectator. Fails (JoinFailed, connection dropped) if no such room, or its
/// game is already decided.
struct JoinRoom {
    std::string name;
};

/// The sender forfeits the game; the server ends the match immediately and
/// awards the win to the opponent. Carries no fields -- who resigned is the
/// connection's own assigned colour. A disconnecting player raises the same
/// outcome, so a dropped connection and a deliberate resign end identically.
struct Resign {};

/// Every message shape a client ever sends.
using ClientMessage = std::variant<Login, Play, CreateRoom, JoinRoom, MoveRequest, JumpRequest, Resign>;

// --- Server -> Client ---

/// Broadcast the instant the server starts a Move/JumpInPlace motion, not on
/// arrival (unlike BoardUpdate) -- lets a networked client predict the
/// glide/jump animation before the real BoardUpdate confirms how it ended.
struct MotionStarted {
    kfc::model::Motion motion;
};

/// Broadcast after any server tick that produced arrivals.
struct BoardUpdate {
    std::vector<kfc::model::ArrivalEvent> arrival_events;
    /// How far the game has got after applying these arrivals; monotonic
    /// within a match, so a client can tell an update it already accounted
    /// for from one it hasn't -- see Welcome::revision.
    std::uint64_t revision = 0;
};

/// Mirrors MoveResult::reason when a Move/JumpRequest was rejected. Never
/// sent for an accepted request -- acceptance is silent, confirmed later by
/// the resulting BoardUpdate.
struct MoveRejected {
    std::string reason;
};

/// winner is std::nullopt for a draw (both kings captured at the exact same
/// simulated instant).
struct GameOver {
    std::optional<kfc::model::PieceColor> winner;
};

/// Broadcast to the still-connected player once per second while a dropped
/// opponent's grace period counts down (20, 19, ..., 1). If the opponent
/// doesn't return in time a GameOver follows; there is no separate
/// "reconnected" message, the countdown simply stops.
struct OpponentDisconnected {
    int seconds_remaining;
};

/// Broadcast the instant both seats of a room are filled and play can begin.
/// A player who joined first sits on Welcome but "searching" until this
/// arrives; the second player gets Welcome and this together.
struct MatchStart {};

/// Sent instead of Welcome when a seating request (Play / CreateRoom /
/// JoinRoom) could not be honoured, carrying why in a stable machine-readable
/// reason (see join_reasons). The connection is closed right after.
struct JoinFailed {
    std::string reason;
};

/// The reasons a JoinFailed can carry. Stable strings, same convention as
/// kfc::model::move_reasons.
namespace join_reasons {
/// JoinRoom named a room that does not exist.
inline constexpr const char* kNoSuchRoom = "no_such_room";
/// The room exists but its game is already decided.
inline constexpr const char* kRoomNotActive = "room_not_active";
/// CreateRoom named a room that already exists.
inline constexpr const char* kRoomNameTaken = "room_name_taken";
/// The room already has as many spectators as it will take -- a resource cap
/// against one attacker opening unbounded watch connections to a room.
inline constexpr const char* kSpectatorLimitReached = "spectator_limit_reached";
}  // namespace join_reasons

/// Sent instead of Welcome or JoinFailed when JoinRoom named a room that is
/// real but lives on a different kfc_server worker. url is that worker's
/// client-facing address; the client reconnects there and resends Login and
/// the seating request. The connection this arrived on is closed right after.
struct JoinRedirect {
    std::string url;
};

/// Sent when Login was rejected, carrying the account store's own reason
/// (e.g. "wrong_password"). The connection is closed right after.
struct LoginFailed {
    std::string reason;
};

/// The reasons a LoginFailed can carry, beyond the account store's own.
/// Same convention as join_reasons.
namespace login_reasons {
/// This username already has a live connection.
inline constexpr const char* kAlreadyLoggedIn = "already_logged_in";
/// Too many Login attempts from this connection's remote IP in the current
/// window; shared with the HTTP login/register budget so splitting an attack
/// across WebSocket and HTTP paths does not double it.
inline constexpr const char* kRateLimited = "rate_limited";
}  // namespace login_reasons

/// Broadcast when a player who dropped came back before their grace ran out,
/// so the opponent's countdown stops. No payload: who returned is whoever the
/// countdown was for, which only the server knows.
struct OpponentReconnected {};

/// Every message shape a client ever receives.
using ServerMessage = std::variant<Welcome, MotionStarted, BoardUpdate, MoveRejected, GameOver, OpponentDisconnected,
                                   MatchStart, JoinFailed, JoinRedirect, LoginFailed, OpponentReconnected>;

}  // namespace kfc::protocol
