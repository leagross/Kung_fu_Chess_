#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include "kfc/model/piece.hpp"
#include "kfc/database/elo.hpp"
#include "kfc/database/rating_service.hpp"
#include "kfc/database/user_repository.hpp"

using namespace kfc::database;
using kfc::model::PieceColor;

namespace {

std::string fresh_db_path() {
    static int counter = 0;
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("kfc_rating_test_" + std::to_string(counter++) + ".db");
    std::filesystem::remove(path);
    return path.string();
}

}  // namespace

TEST(RatingServiceTest, WhiteWinTakesPointsFromBlackAtEqualRatings) {
    UserRepository users(fresh_db_path());
    users.authenticate("white", "pw");  // both start at 1200
    users.authenticate("black", "pw");

    apply_game_result(users, PieceColor::White, "white", "black");

    // Equal ratings, K=32: winner +16, loser -16.
    EXPECT_EQ(*users.rating_of("white"), 1216);
    EXPECT_EQ(*users.rating_of("black"), 1184);
}

TEST(RatingServiceTest, BlackWinIsTheMirror) {
    UserRepository users(fresh_db_path());
    users.authenticate("white", "pw");
    users.authenticate("black", "pw");

    apply_game_result(users, PieceColor::Black, "white", "black");

    EXPECT_EQ(*users.rating_of("white"), 1184);
    EXPECT_EQ(*users.rating_of("black"), 1216);
}

TEST(RatingServiceTest, ADrawLeavesEqualRatingsUnchanged) {
    UserRepository users(fresh_db_path());
    users.authenticate("white", "pw");
    users.authenticate("black", "pw");

    apply_game_result(users, std::nullopt, "white", "black");

    EXPECT_EQ(*users.rating_of("white"), kStartingRating);
    EXPECT_EQ(*users.rating_of("black"), kStartingRating);
}

TEST(RatingServiceTest, UnknownPlayerIsANoOp) {
    UserRepository users(fresh_db_path());
    users.authenticate("white", "pw");  // black never registered

    apply_game_result(users, PieceColor::White, "white", "ghost");

    // white's rating must be untouched -- the result could not be applied.
    EXPECT_EQ(*users.rating_of("white"), kStartingRating);
}

TEST(RatingServiceTest, ForfeitDocksTheFlatDisconnectPenalty) {
    UserRepository users(fresh_db_path());
    users.authenticate("quitter", "pw");

    apply_forfeit(users, "quitter");

    EXPECT_EQ(*users.rating_of("quitter"), kStartingRating - kDisconnectPenalty);  // 1190
}

TEST(RatingServiceTest, ForfeitOfAnUnknownPlayerIsANoOp) {
    UserRepository users(fresh_db_path());
    EXPECT_NO_THROW(apply_forfeit(users, "ghost"));
}

// --- Two games finishing at once must not overwrite each other ---

// Every result is a read-modify-write. Spelled as separate reads and writes,
// two of them landing together lose one result entirely: the second write is
// computed from a rating the first already replaced. Real games do finish at
// the same instant -- each match has its own tick thread, and they all report
// into this one store.
//
// An ELO exchange is exactly zero-sum (the loser's rounded delta is the
// negation of the winner's), so however many games these two play in whatever
// order, their ratings must still add up to what they started with. A lost
// update breaks that sum, which is why it is what this asserts rather than any
// particular final rating.
TEST(RatingServiceTest, ConcurrentResultsForTheSamePairConserveTheirTotal) {
    UserRepository users(fresh_db_path());
    (void)users.authenticate("white", "pw");
    (void)users.authenticate("black", "pw");
    const int total_before = *users.rating_of("white") + *users.rating_of("black");

    constexpr int kThreads = 4;
    constexpr int kGamesPerThread = 25;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&users, t] {
            for (int i = 0; i < kGamesPerThread; ++i) {
                // Alternating, so ratings stay near each other and every game
                // is a real exchange rather than a rounded-to-nothing one.
                PieceColor winner = ((t + i) % 2 == 0) ? PieceColor::White : PieceColor::Black;
                apply_game_result(users, winner, "white", "black");
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(*users.rating_of("white") + *users.rating_of("black"), total_before)
        << "a result was computed from a rating another thread had already replaced";
}

TEST(RatingServiceTest, ConcurrentForfeitsAreAllCharged) {
    UserRepository users(fresh_db_path());
    (void)users.authenticate("quitter", "pw");

    constexpr int kThreads = 4;
    constexpr int kForfeitsPerThread = 25;
    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&users] {
            for (int i = 0; i < kForfeitsPerThread; ++i) {
                apply_forfeit(users, "quitter");
            }
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    // Every penalty is charged exactly once -- none swallowed by a concurrent
    // one reading the rating before it was written.
    EXPECT_EQ(*users.rating_of("quitter"),
              kStartingRating - kThreads * kForfeitsPerThread * kDisconnectPenalty);
}
