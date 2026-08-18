#pragma once

namespace kfc::model::move_reasons {

inline constexpr const char* kOk = "ok";
inline constexpr const char* kOutsideBoard = "outside_board";
inline constexpr const char* kEmptySource = "empty_source";
inline constexpr const char* kFriendlyDestination = "friendly_destination";
inline constexpr const char* kIllegalPieceMove = "illegal_piece_move";
inline constexpr const char* kGameOver = "game_over";
inline constexpr const char* kMotionInProgress = "motion_in_progress";
/// Server-level rejection: the requesting connection isn't the color the
/// piece belongs to. Not a RuleEngine concern -- it has no notion of
/// "which connection asked".
inline constexpr const char* kNotYourPiece = "not_your_piece";
/// Server-level rejection: the opponent disconnected and their grace period
/// hasn't expired, so the match is frozen.
inline constexpr const char* kOpponentDisconnected = "opponent_disconnected";
/// Server-level rejection: only one player is seated, so there is no game
/// to play yet.
inline constexpr const char* kMatchNotStarted = "match_not_started";

}  // namespace kfc::model::move_reasons
