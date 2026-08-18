#pragma once

namespace kfc::database {

/// Initial rating for every new account.
inline constexpr int kStartingRating = 1200;

/// Standard chess K-factor.
inline constexpr int kDefaultKFactor = 32;

/// Flat rating hit for a disconnect/timeout loss, applied instead of an ELO exchange.
inline constexpr int kDisconnectPenalty = 10;

/// Matchmaking only pairs players whose ratings are within this many points.
inline constexpr int kMatchmakingRatingGap = 100;

/// Probability (0..1) that `rating` scores against `opponent_rating`, per the ELO logistic curve.
[[nodiscard]] double elo_expected_score(int rating, int opponent_rating);

/// New rating after a game scored `score` (1.0 win, 0.5 draw, 0.0 loss) against `opponent_rating`.
[[nodiscard]] int elo_updated_rating(int rating, int opponent_rating, double score, int k = kDefaultKFactor);

}  // namespace kfc::database
