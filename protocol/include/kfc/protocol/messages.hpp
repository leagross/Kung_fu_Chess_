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

/// Wire/snapshot-only flattening of a Board's occupancy.
struct BoardSnapshot {
    int width;
    int height;
    std::vector<kfc::model::Piece> pieces;
};

/// Sent once after a successful Login. spectator marks a viewer; a client
/// must read it (not assigned_color) to decide if it may play.
struct Welcome {
    kfc::model::PieceColor assigned_color;
    BoardSnapshot board;
    bool spectator = false;
    /// Empty for a Play (matchmaking) room; authoritative for Join.
    std::string room;
    /// Every arrival this match has already seen, oldest first.
    std::vector<kfc::model::ArrivalEvent> history;
    /// See BoardUpdate::revision; a joiner may see an update its own
    /// snapshot already contains, but never miss one.
    std::uint64_t revision = 0;
    /// Both seats' usernames/ratings; empty/0 until that seat is filled.
    std::string white_username;
    std::string black_username;
    int white_rating = 0;
    int black_rating = 0;
};

// --- Client -> Server ---

/// First login for a username registers it; later logins must match.
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

/// Post-login "find me any opponent": seats the sender into matchmaking.
struct Play {};

/// Post-login "open a room for me": server mints the id and seats sender as White.
struct CreateRoom {};

/// Post-login "join the room named `name`": Black if first opponent, else spectator.
struct JoinRoom {
    std::string name;
};

/// Forfeits the game; who resigned is the connection's own colour.
struct Resign {};

using ClientMessage = std::variant<Login, Play, CreateRoom, JoinRoom, MoveRequest, JumpRequest, Resign>;

// --- Server -> Client ---

/// Broadcast the instant a Move/JumpInPlace starts, not on arrival, so a
/// client can predict the glide/jump animation before BoardUpdate confirms it.
struct MotionStarted {
    kfc::model::Motion motion;
};

/// Broadcast after any server tick that produced arrivals.
struct BoardUpdate {
    std::vector<kfc::model::ArrivalEvent> arrival_events;
    /// Monotonic within a match; see Welcome::revision.
    std::uint64_t revision = 0;
};

/// Mirrors MoveResult::reason; never sent for an accepted request.
struct MoveRejected {
    std::string reason;
};

/// winner is nullopt for a draw (both kings captured simultaneously).
struct GameOver {
    std::optional<kfc::model::PieceColor> winner;
};

/// Broadcast once per second during a dropped opponent's grace countdown.
struct OpponentDisconnected {
    int seconds_remaining;
};

/// Broadcast once both seats are filled and play can begin.
struct MatchStart {
    std::string white_username;
    std::string black_username;
    int white_rating = 0;
    int black_rating = 0;
};

/// Sent instead of Welcome when a seating request could not be honoured;
/// the connection is closed right after.
struct JoinFailed {
    std::string reason;
};

/// Stable machine-readable reasons for JoinFailed.
namespace join_reasons {
inline constexpr const char* kNoSuchRoom = "no_such_room";
inline constexpr const char* kRoomNotActive = "room_not_active";
inline constexpr const char* kRoomNameTaken = "room_name_taken";
/// Resource cap against unbounded watch connections to one room.
inline constexpr const char* kSpectatorLimitReached = "spectator_limit_reached";
/// Own budget, separate from login_reasons::kRateLimited.
inline constexpr const char* kRateLimited = "rate_limited";
}  // namespace join_reasons

/// Sent when JoinRoom named a room living on a different kfc_server worker;
/// client reconnects to url and resends Login plus the seating request.
struct JoinRedirect {
    std::string url;
};

/// Sent when Login was rejected; connection is closed right after.
struct LoginFailed {
    std::string reason;
};

namespace login_reasons {
inline constexpr const char* kAlreadyLoggedIn = "already_logged_in";
/// Shared with the HTTP login/register budget.
inline constexpr const char* kRateLimited = "rate_limited";
}  // namespace login_reasons

/// Broadcast when a dropped player returns before their grace ran out.
struct OpponentReconnected {};

using ServerMessage = std::variant<Welcome, MotionStarted, BoardUpdate, MoveRejected, GameOver, OpponentDisconnected,
                                   MatchStart, JoinFailed, JoinRedirect, LoginFailed, OpponentReconnected>;

}  // namespace kfc::protocol
