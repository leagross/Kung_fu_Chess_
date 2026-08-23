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

/// One finished game, as recorded for GET /api/history/{username}.
struct GameRecord {
    long long id = 0;
    std::string white_username;
    std::string black_username;
    std::optional<std::string> winner_username;  // nullopt = draw
    std::string end_reason;                       // "decisive" | "draw" | "disconnect"
    std::string started_at;                       // ISO-8601 UTC
    std::string ended_at;                          // ISO-8601 UTC
};

/// SQLite-backed IUserStore: username, Argon2id hash, ELO rating per row. Single-machine only.
class UserRepository : public IUserStore {
public:
    /// Opens (creating if needed) db_path and ensures the schema exists. ":memory:" for tests.
    explicit UserRepository(const std::string& db_path);
    ~UserRepository() override;
    UserRepository(const UserRepository&) = delete;
    UserRepository& operator=(const UserRepository&) = delete;

    /// Registers username on first sight, else verifies against the stored hash.
    [[nodiscard]] AuthOutcome authenticate(const std::string& username, const std::string& password) override;

    [[nodiscard]] std::optional<int> rating_of(const std::string& username) override;

    /// Overwrites the rating directly; use rerate/rerate_pair instead if the new value depends on the old one.
    void set_rating(const std::string& username, int rating);

    [[nodiscard]] bool rerate_pair(const std::string& first, const std::string& second,
                                   const std::function<std::pair<int, int>(int, int)>& compute) override;

    [[nodiscard]] bool rerate(const std::string& username, const std::function<int(int)>& compute) override;

    /// Unlike authenticate(), never creates an account -- distinguishes "no such user" from "wrong password".
    [[nodiscard]] bool user_exists(const std::string& username);

    void record_game(const std::string& white_username, const std::string& black_username,
                     std::optional<std::string> winner_username, const std::string& end_reason,
                     std::chrono::system_clock::time_point started_at,
                     std::chrono::system_clock::time_point ended_at);

    /// Newest ended_at first; empty (not an error) for an unknown username.
    [[nodiscard]] std::vector<GameRecord> history_for(const std::string& username);

private:
    // Both assume mutex_ is already held by the caller.
    [[nodiscard]] std::optional<int> read_rating(const std::string& username);
    void write_rating(const std::string& username, int rating);

    std::mutex mutex_;
    std::unique_ptr<SQLite::Database> db_;
};

}  // namespace kfc::database
