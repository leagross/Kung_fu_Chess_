#pragma once

namespace kfc::model::move_reasons {

inline constexpr const char* kOk = "ok";
inline constexpr const char* kOutsideBoard = "outside_board";
inline constexpr const char* kEmptySource = "empty_source";
inline constexpr const char* kFriendlyDestination = "friendly_destination";
inline constexpr const char* kIllegalPieceMove = "illegal_piece_move";
inline constexpr const char* kGameOver = "game_over";
inline constexpr const char* kMotionInProgress = "motion_in_progress";
inline constexpr const char* kNotYourPiece = "not_your_piece";  // server-level: wrong connection for this piece's color
inline constexpr const char* kOpponentDisconnected = "opponent_disconnected";
inline constexpr const char* kMatchNotStarted = "match_not_started";

}  // namespace kfc::model::move_reasons
