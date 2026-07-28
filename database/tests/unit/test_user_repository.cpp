#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

#include "kfc/database/elo.hpp"
#include "kfc/database/user_repository.hpp"

using kfc::database::kStartingRating;
using kfc::database::UserRepository;

namespace {

// Each test gets its own fresh file so state never leaks between tests; a
// ":memory:" DB would also work but a file lets one test inspect the raw table
// through a second connection (see the plaintext test).
std::string fresh_db_path() {
    static int counter = 0;
    std::filesystem::path path =
        std::filesystem::temp_directory_path() / ("kfc_users_test_" + std::to_string(counter++) + ".db");
    std::filesystem::remove(path);
    return path.string();
}

}  // namespace

TEST(UserRepositoryTest, FirstLoginRegistersAtStartingRating) {
    UserRepository repo(fresh_db_path());

    UserRepository::AuthOutcome outcome = repo.authenticate("alice", "hunter2");

    EXPECT_TRUE(outcome.ok);
    EXPECT_TRUE(outcome.newly_registered);
    EXPECT_EQ(outcome.rating, kStartingRating);
}

TEST(UserRepositoryTest, ReturningWithTheCorrectPasswordSucceeds) {
    UserRepository repo(fresh_db_path());
    repo.authenticate("alice", "hunter2");

    UserRepository::AuthOutcome outcome = repo.authenticate("alice", "hunter2");

    EXPECT_TRUE(outcome.ok);
    EXPECT_FALSE(outcome.newly_registered);  // second time is a login, not a sign-up
    EXPECT_EQ(outcome.rating, kStartingRating);
}

TEST(UserRepositoryTest, WrongPasswordIsRejected) {
    UserRepository repo(fresh_db_path());
    repo.authenticate("alice", "hunter2");

    UserRepository::AuthOutcome outcome = repo.authenticate("alice", "guess");

    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(outcome.reason, "wrong_password");
}

TEST(UserRepositoryTest, PasswordIsStoredHashedNotInPlaintext) {
    std::string path = fresh_db_path();
    {
        UserRepository repo(path);
        repo.authenticate("alice", "hunter2");
    }

    // Read the raw row through a separate connection: the plaintext must not be
    // anywhere in it, and the hash must be a 64-char hex digest.
    SQLite::Database raw(path, SQLite::OPEN_READONLY);
    SQLite::Statement query(raw, "SELECT password_hash FROM users WHERE username = ?");
    query.bind(1, "alice");
    ASSERT_TRUE(query.executeStep());
    std::string stored = query.getColumn(0).getString();

    EXPECT_NE(stored, "hunter2");
    EXPECT_EQ(stored.size(), 64u);
}

TEST(UserRepositoryTest, RatingOfUnknownUserIsNullopt) {
    UserRepository repo(fresh_db_path());
    EXPECT_FALSE(repo.rating_of("nobody").has_value());
}

TEST(UserRepositoryTest, SetRatingPersistsAndShowsUpOnNextLogin) {
    UserRepository repo(fresh_db_path());
    repo.authenticate("alice", "hunter2");

    repo.set_rating("alice", 1337);

    ASSERT_TRUE(repo.rating_of("alice").has_value());
    EXPECT_EQ(*repo.rating_of("alice"), 1337);
    EXPECT_EQ(repo.authenticate("alice", "hunter2").rating, 1337);
}

TEST(UserRepositoryTest, AccountsAreIndependent) {
    UserRepository repo(fresh_db_path());
    repo.authenticate("alice", "apw");
    repo.authenticate("bob", "bpw");

    // Cross passwords are rejected; each keeps its own rating.
    EXPECT_FALSE(repo.authenticate("alice", "bpw").ok);
    repo.set_rating("alice", 1300);
    EXPECT_EQ(*repo.rating_of("bob"), kStartingRating);
}
