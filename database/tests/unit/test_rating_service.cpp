#include <gtest/gtest.h>

#include <filesystem>
#include <string>

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
