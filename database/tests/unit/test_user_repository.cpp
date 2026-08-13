#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>

#include "kfc/database/elo.hpp"
#include "kfc/database/password_hash.hpp"
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

TEST(UserRepositoryTest, ATooShortNewUsernameIsRejectedAndNothingIsCreated) {
    UserRepository repo(fresh_db_path());

    UserRepository::AuthOutcome outcome = repo.authenticate("ab", "hunter2");

    EXPECT_FALSE(outcome.ok);
    EXPECT_FALSE(outcome.newly_registered);
    EXPECT_EQ(outcome.reason, "invalid_username");
    EXPECT_FALSE(repo.user_exists("ab"));
}

TEST(UserRepositoryTest, ANewUsernameWithASpaceIsRejected) {
    UserRepository repo(fresh_db_path());

    UserRepository::AuthOutcome outcome = repo.authenticate("bad name", "hunter2");

    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(outcome.reason, "invalid_username");
}

TEST(UserRepositoryTest, ATooShortNewPasswordIsRejectedAndTheAccountIsNotCreated) {
    UserRepository repo(fresh_db_path());

    UserRepository::AuthOutcome outcome = repo.authenticate("alice", "short");

    EXPECT_FALSE(outcome.ok);
    EXPECT_FALSE(outcome.newly_registered);
    EXPECT_EQ(outcome.reason, "weak_password");
    EXPECT_FALSE(repo.user_exists("alice"))
        << "a rejected registration must not leave the username claimed";
}

TEST(UserRepositoryTest, RejectingANewAccountLeavesTheUsernameFreeToTryAgain) {
    UserRepository repo(fresh_db_path());
    repo.authenticate("alice", "short");  // rejected -- see the test above

    UserRepository::AuthOutcome retry = repo.authenticate("alice", "a-real-password");

    EXPECT_TRUE(retry.ok);
    EXPECT_TRUE(retry.newly_registered);
}

TEST(UserRepositoryTest, UsernameAndPasswordRulesDoNotApplyToAnExistingAccounts) {
    // The rules only gate creating a *new* account. A username or password
    // that predates them (or a login attempt with the wrong password, which
    // happens to be short) must keep working exactly as it always did.
    UserRepository repo(fresh_db_path());
    repo.authenticate("alice", "hunter2");

    UserRepository::AuthOutcome outcome = repo.authenticate("alice", "x");

    EXPECT_FALSE(outcome.ok);
    EXPECT_EQ(outcome.reason, "wrong_password") << "a short guess is still just a wrong password here, "
                                                   "not a rejected registration";
}

// http_api.cpp's handle_register used to be user_exists() followed by a
// separate authenticate() call -- two locked operations with a window between
// them where two concurrent registrations of the same brand-new username
// could both see user_exists() == false, then both reach authenticate()'s
// insert; the second would throw on username's PRIMARY KEY constraint,
// uncaught, on an HTTP connection thread. The fix was to call authenticate()
// alone and branch on newly_registered, relying on it being one atomic,
// locked lookup-or-insert. This is what actually proves that: many distinct
// usernames, each raced by two threads at once, must produce exactly one
// newly_registered == true per username and never throw.
TEST(UserRepositoryTest, ConcurrentAuthenticateForTheSameNewUsernameRegistersExactlyOnce) {
    UserRepository repo(fresh_db_path());
    constexpr int kUsernames = 40;

    std::vector<std::thread> threads;
    threads.reserve(kUsernames * 2);
    std::vector<std::atomic<int>> created_counts(kUsernames);
    for (auto& count : created_counts) {
        count = 0;
    }

    for (int i = 0; i < kUsernames; ++i) {
        std::string username = "racer" + std::to_string(i);
        for (int copy = 0; copy < 2; ++copy) {
            threads.emplace_back([&repo, &created_counts, i, username] {
                UserRepository::AuthOutcome outcome = repo.authenticate(username, "hunter2");
                if (outcome.newly_registered) {
                    created_counts[i].fetch_add(1);
                }
            });
        }
    }
    for (std::thread& thread : threads) {
        thread.join();
    }

    for (int i = 0; i < kUsernames; ++i) {
        EXPECT_EQ(created_counts[i].load(), 1) << "username racer" << i;
    }
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
    // anywhere in it, and what is there must be an Argon2id credential.
    SQLite::Database raw(path, SQLite::OPEN_READONLY);
    SQLite::Statement query(raw, "SELECT salt, password_hash FROM users WHERE username = ?");
    query.bind(1, "alice");
    ASSERT_TRUE(query.executeStep());
    std::string salt_column = query.getColumn(0).getString();
    std::string stored = query.getColumn(1).getString();

    EXPECT_EQ(stored.find("hunter2"), std::string::npos) << "the password is in the row: " << stored;
    // The PHC string names the algorithm and the cost it was made with, so an
    // old hash stays verifiable after the parameters are raised.
    EXPECT_EQ(stored.rfind("$argon2id$v=19$", 0), 0u) << "not an Argon2id credential: " << stored;
    EXPECT_NE(stored.find("m=" + std::to_string(kfc::database::password_hash::kMemoryKiB)), std::string::npos);
    EXPECT_NE(stored.find("t=" + std::to_string(kfc::database::password_hash::kIterations)), std::string::npos);
    // Argon2 carries its own salt inside that string, so the separate column
    // this schema used to need is now dead weight rather than a second value to
    // keep in step with the hash.
    EXPECT_TRUE(salt_column.empty());
}

// Two accounts choosing the same password must not produce the same stored
// credential -- otherwise one cracked hash would open every account that shares
// it, and the whole point of a per-account salt is lost.
TEST(UserRepositoryTest, TheSamePasswordStoresDifferentlyForDifferentAccounts) {
    std::string path = fresh_db_path();
    {
        UserRepository repo(path);
        (void)repo.authenticate("alice", "same-password");
        (void)repo.authenticate("bob", "same-password");
        EXPECT_TRUE(repo.authenticate("alice", "same-password").ok) << "the stored credential stopped verifying";
        EXPECT_FALSE(repo.authenticate("alice", "same-passwore").ok) << "a near-miss authenticated";
    }

    SQLite::Database raw(path, SQLite::OPEN_READONLY);
    SQLite::Statement query(raw, "SELECT username, password_hash FROM users ORDER BY username");
    std::vector<std::string> stored;
    while (query.executeStep()) {
        stored.push_back(query.getColumn(1).getString());
    }

    ASSERT_EQ(stored.size(), 2u);
    EXPECT_NE(stored[0], stored[1])
        << "two accounts with one password share a credential -- cracking one opens both";
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
    repo.authenticate("alice", "apassword");
    repo.authenticate("bob", "bpassword");

    // Cross passwords are rejected; each keeps its own rating.
    EXPECT_FALSE(repo.authenticate("alice", "bpassword").ok);
    repo.set_rating("alice", 1300);
    EXPECT_EQ(*repo.rating_of("bob"), kStartingRating);
}

TEST(UserRepositoryTest, UserExistsIsFalseUntilRegisteredAndDoesNotRegister) {
    UserRepository repo(fresh_db_path());

    EXPECT_FALSE(repo.user_exists("alice"));
    EXPECT_FALSE(repo.rating_of("alice").has_value())
        << "user_exists must not have side-effected a registration, unlike authenticate()";

    repo.authenticate("alice", "hunter2");
    EXPECT_TRUE(repo.user_exists("alice"));
}

TEST(UserRepositoryTest, HistoryForUnknownUserIsEmptyNotAnError) {
    UserRepository repo(fresh_db_path());
    EXPECT_TRUE(repo.history_for("nobody").empty());
}

TEST(UserRepositoryTest, RecordGameShowsUpForBothPlayersNewestFirst) {
    using namespace std::chrono;

    UserRepository repo(fresh_db_path());
    auto t0 = system_clock::now();

    repo.record_game("alice", "bob", "alice", "decisive", t0, t0 + seconds(30));
    repo.record_game("alice", "carol", std::nullopt, "draw", t0 + seconds(60), t0 + seconds(90));

    std::vector<kfc::database::GameRecord> alice_games = repo.history_for("alice");
    ASSERT_EQ(alice_games.size(), 2u);
    // Newest ended_at first.
    EXPECT_EQ(alice_games[0].black_username, "carol");
    EXPECT_FALSE(alice_games[0].winner_username.has_value()) << "a draw must serialize winner as absent";
    EXPECT_EQ(alice_games[0].end_reason, "draw");
    EXPECT_EQ(alice_games[1].black_username, "bob");
    ASSERT_TRUE(alice_games[1].winner_username.has_value());
    EXPECT_EQ(*alice_games[1].winner_username, "alice");
    EXPECT_EQ(alice_games[1].end_reason, "decisive");

    // bob only played one of the two games -- found via black_username, same
    // as alice was found via white_username above.
    std::vector<kfc::database::GameRecord> bob_games = repo.history_for("bob");
    ASSERT_EQ(bob_games.size(), 1u);
    EXPECT_EQ(bob_games[0].white_username, "alice");
}
