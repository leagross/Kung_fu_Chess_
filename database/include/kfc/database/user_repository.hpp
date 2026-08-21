#pragma once

#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "kfc/database/user_store.hpp"

namespace SQLite {
class Database;
}

namespace kfc::database {

/// One finished game, as recorded for GET /api/history/{username}. Lives here
/// rather than on IUserStore since it's a SQLite-specific extra, not part of
/// the narrow authenticate/rate contract.
struct GameRecord {
    long long id = 0;
    std::string white_username;
    std::string black_username;
    std::optional<std::string> winner_username;  // nullopt = draw
    std::string end_reason;                       // "decisive" | "draw" | "disconnect"
    std::string started_at;                       // ISO-8601 UTC, ready for JSON
    std::string ended_at;                          // ISO-8601 UTC, ready for JSON
};

/// Server-side account store, backed by a single SQLite file: username,
/// Argon2id password hash, ELO rating per row. SQLite implementation of
/// IUserStore -- fine for one machine, not the scale Server_Design.md
/// targets, which is why callers hold the interface, not this class.
class UserRepository : public IUserStore {
public:
    /// Opens (creating if needed) the SQLite database at db_path and ensures
    /// the users table exists. Pass ":memory:" for a throwaway in-process DB
    /// (tests). Throws if the database cannot be opened.
    explicit UserRepository(const std::string& db_path);
    ~UserRepository() override;
    UserRepository(const UserRepository&) = delete;
    UserRepository& operator=(const UserRepository&) = delete;

    /// Registers username with password on first sight (rating =
    /// kStartingRating), or verifies against the stored hash on return.
    /// reason is "wrong_password", "invalid_username" or "weak_password".
    [[nodiscard]] AuthOutcome authenticate(const std::string& username, const std::string& password) override;

    /// The user's current rating, or std::nullopt if there is no such user.
    [[nodiscard]] std::optional<int> rating_of(const std::string& username) override;

    /// Overwrites the user's rating. A no-op if the user does not exist.
    /// Only safe when the new value doesn't depend on the current one --
    /// otherwise use rerate/rerate_pair to avoid a lost update.
    void set_rating(const std::string& username, int rating);

    /// Reads both current ratings, hands them to compute in that order, and
    /// writes back the pair it returns, all under one lock to avoid a lost
    /// update between concurrent games. False (nothing written) if either
    /// user is unknown. compute must not call back into this repository.
    [[nodiscard]] bool rerate_pair(const std::string& first, const std::string& second,
                                   const std::function<std::pair<int, int>(int, int)>& compute) override;

    /// One-player form of rerate_pair, for a forfeit penalty. False if unknown.
    [[nodiscard]] bool rerate(const std::string& username, const std::function<int(int)>& compute) override;

    /// True if username has an account. Unlike authenticate(), never creates
    /// one -- lets callers tell "no such user" apart from "wrong password".
    [[nodiscard]] bool user_exists(const std::string& username);

    /// Persists one finished game. winner_username is std::nullopt for a draw.
    void record_game(const std::string& white_username, const std::string& black_username,
                     std::optional<std::string> winner_username, const std::string& end_reason,
                     std::chrono::system_clock::time_point started_at,
                     std::chrono::system_clock::time_point ended_at);

    /// Every game username played as either colour, newest ended_at first.
    /// Empty (not an error) for an unknown username.
    [[nodiscard]] std::vector<GameRecord> history_for(const std::string& username);

private:
    // Both assume mutex_ is already held by the caller.
    [[nodiscard]] std::optional<int> read_rating(const std::string& username);
    void write_rating(const std::string& username, int rating);

    // Serializes all DB access across connection and match-tick threads.
    std::mutex mutex_;
    std::unique_ptr<SQLite::Database> db_;
};

}  // namespace kfc::database
