#pragma once

namespace kfc::model::move_reasons {

inline constexpr const char* kOk = "ok";
inline constexpr const char* kOutsideBoard = "outside_board";
inline constexpr const char* kEmptySource = "empty_source";
inline constexpr const char* kFriendlyDestination = "friendly_destination";
inline constexpr const char* kIllegalPieceMove = "illegal_piece_move";
inline constexpr const char* kGameOver = "game_over";
inline constexpr const char* kMotionInProgress = "motion_in_progress";
/// Server-level rejection: the connection requesting this move/jump is not
/// the color the piece at that cell belongs to (see server::Match::apply).
/// Not a RuleEngine concern -- RuleEngine has no notion of "which network
/// connection asked", only GameEngine's callers do.
inline constexpr const char* kNotYourPiece = "not_your_piece";

}  // namespace kfc::model::move_reasons
