#include <gtest/gtest.h>

#include "kfc/database/elo.hpp"

using namespace kfc::database;

TEST(EloTest, EqualRatingsExpectHalf) {
    EXPECT_NEAR(elo_expected_score(1500, 1500), 0.5, 1e-9);
}

TEST(EloTest, ExpectedScoreRisesWithRatingEdge) {
    // A 400-point edge is the textbook 10:1 odds -> ~0.909 expected.
    EXPECT_NEAR(elo_expected_score(1600, 1200), 0.909090, 1e-5);
    EXPECT_NEAR(elo_expected_score(1200, 1600), 0.090909, 1e-5);
}

TEST(EloTest, BeatingAStrongerOpponentGainsMoreThanBeatingAWeakerOne) {
    // The spec's own worked examples: 1200 beating 1600 rises far more than
    // 1200 beating 1250.
    int over_1600 = elo_updated_rating(1200, 1600, 1.0);
    int over_1250 = elo_updated_rating(1200, 1250, 1.0);

    EXPECT_EQ(over_1600, 1229);  // +29
    EXPECT_EQ(over_1250, 1218);  // +18
    EXPECT_GT(over_1600 - 1200, over_1250 - 1200);
}

TEST(EloTest, WinnerGainMirrorsLoserLossForAGivenPairing) {
    int winner = elo_updated_rating(1200, 1600, 1.0);  // 1229 (+29)
    int loser = elo_updated_rating(1600, 1200, 0.0);   // 1571 (-29)

    EXPECT_EQ(winner - 1200, -(loser - 1600));
}

TEST(EloTest, ADrawBarelyMovesNearlyEqualRatings) {
    EXPECT_EQ(elo_updated_rating(1200, 1250, 0.5), 1202);  // +2
    EXPECT_EQ(elo_updated_rating(1250, 1200, 0.5), 1248);  // -2
}

TEST(EloTest, StartingRatingIsTwelveHundred) {
    EXPECT_EQ(kStartingRating, 1200);
}
