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

/// One finished game, as recorded for GET /api/history/{username}. Lives
/// alongside UserRepository rather than on IUserStore: history is a concrete,
/// SQLite-specific extra the HTTP API needs, not part of the narrow
/// authenticate/rate contract every account-store implementation must offer.
struct GameRecord {
    long long id = 0;
    std::string white_username;
    std::string black_username;
    std::optional<std::string> winner_username;  // nullopt = draw
    std::string end_reason;                       // "decisive" | "draw" | "disconnect"
    std::string started_at;                       // ISO-8601 UTC, ready for JSON
    std::string ended_at;                          // ISO-8601 UTC, ready for JSON
};

/// Server-side account store, backed by a single SQLite file (the CTD SERVER
/// spec's "SQLI Database ... קובץ בודד, לא צריך התקנות"). One row per user: a
/// per-user random salt, the salted SHA-256 of their password (never the
/// plaintext -- see sha256.hpp), and their ELO rating.
///
/// Password policy is exactly the spec's: the first time a username is seen its
/// password is registered; every later login must match it. Rating starts at
/// kStartingRating (see elo.hpp) and is nudged by ELO after each game.
///
/// This is the SQLite implementation of IUserStore. It is the right one for a
/// single machine and the wrong one for the load Server_Design.md targets --
/// which is exactly why callers hold the interface rather than this class.
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
    /// kStartingRating), or verifies password against the stored hash on a
    /// return visit. reason is "wrong_password" when an existing user's
    /// password does not match.
    [[nodiscard]] AuthOutcome authenticate(const std::string& username, const std::string& password) override;

    /// The user's current rating, or std::nullopt if there is no such user.
    [[nodiscard]] std::optional<int> rating_of(const std::string& username) override;

    /// Overwrites the user's rating. A no-op if the user does not exist.
    ///
    /// Only safe for a rating that does not depend on the current one. Anything
    /// of the form "read it, adjust it, write it back" must use the two methods
    /// below instead -- see why there.
    void set_rating(const std::string& username, int rating);

    /// Re-rates two players as one indivisible step: reads both current
    /// ratings, hands them to compute in that order, and writes back the pair
    /// it returns. False (and nothing written) if either user is unknown.
    ///
    /// This exists because the obvious spelling -- read both, compute, write
    /// both -- is a lost update. Between the read and the write another thread
    /// can finish a game involving one of the same players, and whichever write
    /// lands second silently overwrites the other's result: rating points
    /// appear from nowhere or vanish. The window is small and the bug is
    /// invisible, which is exactly what makes it worth closing in the one place
    /// it can be closed rather than at each call site.
    ///
    /// compute runs with the store's lock held, so it must not call back into
    /// this repository. It is pure arithmetic (see elo.hpp) precisely so that
    /// it can be.
    [[nodiscard]] bool rerate_pair(const std::string& first, const std::string& second,
                                   const std::function<std::pair<int, int>(int, int)>& compute) override;

    /// The one-player form of rerate_pair, for an adjustment that depends on
    /// the player's current rating (a forfeit penalty). False if unknown.
    [[nodiscard]] bool rerate(const std::string& username, const std::function<int(int)>& compute) override;

    /// True if username has an account. Unlike authenticate(), never creates
    /// one -- this is what lets the HTTP API's register/login tell "no such
    /// user" apart from "wrong password" without changing authenticate()'s
    /// own auto-register behaviour, which the WebSocket login flow relies on.
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
    // Both assume mutex_ is already held: the compound operations above must do
    // their read and their write without letting go in between, which is the
    // whole point of them.
    [[nodiscard]] std::optional<int> read_rating(const std::string& username);
    void write_rating(const std::string& username, int rating);

    // Serializes all DB access: the server calls authenticate() from many
    // IXWebSocket connection threads at once, and set_rating() from a match's
    // tick thread -- one mutex around every public method keeps that safe
    // regardless of how SQLite itself was compiled for threading.
    std::mutex mutex_;
    std::unique_ptr<SQLite::Database> db_;
};

}  // namespace kfc::database
