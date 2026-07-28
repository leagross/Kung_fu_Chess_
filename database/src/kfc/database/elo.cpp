#include "kfc/database/elo.hpp"

#include <cmath>

namespace kfc::database {

double elo_expected_score(int rating, int opponent_rating) {
    return 1.0 / (1.0 + std::pow(10.0, (opponent_rating - rating) / 400.0));
}

int elo_updated_rating(int rating, int opponent_rating, double score, int k) {
    double expected = elo_expected_score(rating, opponent_rating);
    return rating + static_cast<int>(std::lround(k * (score - expected)));
}

}  // namespace kfc::database
