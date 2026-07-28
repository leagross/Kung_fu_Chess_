#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>

namespace SQLite {
class Database;
}

namespace kfc::database {

/// Server-side account store, backed by a single SQLite file (the CTD SERVER
/// spec's "SQLI Database ... קובץ בודד, לא צריך התקנות"). One row per user: a
/// per-user random salt, the salted SHA-256 of their password (never the
/// plaintext -- see sha256.hpp), and their ELO rating.
///
/// Password policy is exactly the spec's: the first time a username is seen its
/// password is registered; every later login must match it. Rating starts at
/// kStartingRating (see elo.hpp) and is nudged by ELO after each game.
class UserRepository {
public:
    /// Opens (creating if needed) the SQLite database at db_path and ensures
    /// the users table exists. Pass ":memory:" for a throwaway in-process DB
    /// (tests). Throws if the database cannot be opened.
    explicit UserRepository(const std::string& db_path);
    ~UserRepository();
    UserRepository(const UserRepository&) = delete;
    UserRepository& operator=(const UserRepository&) = delete;

    struct AuthOutcome {
        bool ok = false;
        std::string reason;             // empty on success; else e.g. "wrong_password"
        int rating = 0;                 // the account's rating, valid when ok
        bool newly_registered = false;  // true if this call created the account
    };

    /// Registers username with password on first sight (rating =
    /// kStartingRating), or verifies password against the stored hash on a
    /// return visit. reason is "wrong_password" when an existing user's
    /// password does not match.
    [[nodiscard]] AuthOutcome authenticate(const std::string& username, const std::string& password);

    /// The user's current rating, or std::nullopt if there is no such user.
    [[nodiscard]] std::optional<int> rating_of(const std::string& username);

    /// Overwrites the user's rating. A no-op if the user does not exist.
    void set_rating(const std::string& username, int rating);

private:
    // Serializes all DB access: the server calls authenticate() from many
    // IXWebSocket connection threads at once, and set_rating() from a match's
    // tick thread -- one mutex around every public method keeps that safe
    // regardless of how SQLite itself was compiled for threading.
    std::mutex mutex_;
    std::unique_ptr<SQLite::Database> db_;
};

}  // namespace kfc::database
