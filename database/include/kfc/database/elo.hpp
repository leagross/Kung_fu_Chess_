#pragma once

namespace kfc::database {

/// Every new account starts here (CTD SERVER spec: "הרייטינג ההתחלתי של כולם
/// יהיה 1,200").
inline constexpr int kStartingRating = 1200;

/// Standard chess K-factor. Reproduces the spec's own worked examples: a 1200
/// beating a 1600 gains far more (+29) than a 1200 beating a 1250 (+18).
inline constexpr int kDefaultKFactor = 32;

/// Flat rating hit for losing a game by disconnect/timeout, kept deliberately
/// separate from the ELO exchange below (CTD SERVER spec: a mid-game
/// disconnect auto-loses and docks the rating by 10, a fixed forfeit penalty
/// rather than a rating-dependent ELO swing).
inline constexpr int kDisconnectPenalty = 10;

/// Play-button matchmaking only pairs two players whose ratings are within this
/// many points of each other (CTD SERVER spec: "רייטינג פלוס מינוס מאה").
inline constexpr int kMatchmakingRatingGap = 100;

/// Probability (0..1) that a player rated `rating` scores against a player
/// rated `opponent_rating`, per the standard ELO logistic curve. Equal ratings
/// give 0.5; a large edge tends toward 1.0.
[[nodiscard]] double elo_expected_score(int rating, int opponent_rating);

/// The player's new rating after a game they scored `score` in -- 1.0 for a
/// win, 0.5 for a draw, 0.0 for a loss -- against `opponent_rating`. The delta
/// is k * (score - expected), rounded to the nearest whole rating point, so
/// beating a stronger opponent is worth more than beating a weaker one.
[[nodiscard]] int elo_updated_rating(int rating, int opponent_rating, double score, int k = kDefaultKFactor);

}  // namespace kfc::database
