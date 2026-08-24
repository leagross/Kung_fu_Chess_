#pragma once

namespace kfc::database {

inline constexpr int kStartingRating = 1200;
inline constexpr int kDefaultKFactor = 32;

/// Flat penalty applied instead of a normal ELO exchange.
inline constexpr int kDisconnectPenalty = 10;

/// Matchmaking only pairs ratings within this gap.
inline constexpr int kMatchmakingRatingGap = 100;

/// Probability (0..1) that `rating` scores against `opponent_rating`, per the ELO logistic curve.
[[nodiscard]] double elo_expected_score(int rating, int opponent_rating);

/// New rating after a game scored `score` (1.0 win, 0.5 draw, 0.0 loss) against `opponent_rating`.
[[nodiscard]] int elo_updated_rating(int rating, int opponent_rating, double score, int k = kDefaultKFactor);

}  // namespace kfc::database
