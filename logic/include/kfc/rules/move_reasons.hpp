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
/// Server-level rejection: the opponent's connection dropped and their grace
/// period is still counting down, so the match is frozen and nobody may move
/// until they return or the grace expires (see server::Match::apply). Like
/// kNotYourPiece, this is about the connection rather than the rules.
inline constexpr const char* kOpponentDisconnected = "opponent_disconnected";
/// Server-level rejection: only one player is seated, so there is no game to
/// play yet (see server::MatchState::Waiting). Without this a player waiting to
/// be matched could move their pieces around, and their opponent would arrive
/// into a game already under way.
inline constexpr const char* kMatchNotStarted = "match_not_started";

}  // namespace kfc::model::move_reasons
