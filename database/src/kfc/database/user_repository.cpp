#include "kfc/database/user_repository.hpp"

#include <random>

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h>

#include "kfc/database/elo.hpp"
#include "kfc/database/sha256.hpp"

namespace kfc::database {

namespace {

// 16 random bytes, hex-encoded. Per-user, so two accounts with the same
// password still hash differently and a stolen DB can't be cracked with a
// single precomputed table.
std::string random_salt() {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<int> nibble(0, 15);
    static const char* kHex = "0123456789abcdef";
    std::string salt;
    salt.reserve(32);
    for (int i = 0; i < 32; ++i) {
        salt.push_back(kHex[nibble(rng)]);
    }
    return salt;
}

}  // namespace

UserRepository::UserRepository(const std::string& db_path)
    : db_(std::make_unique<SQLite::Database>(db_path, SQLite::OPEN_READWRITE | SQLite::OPEN_CREATE)) {
    db_->exec(
        "CREATE TABLE IF NOT EXISTS users ("
        "  username TEXT PRIMARY KEY,"
        "  salt TEXT NOT NULL,"
        "  password_hash TEXT NOT NULL,"
        "  rating INTEGER NOT NULL"
        ")");
}

UserRepository::~UserRepository() = default;

UserRepository::AuthOutcome UserRepository::authenticate(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> guard(mutex_);
    SQLite::Statement lookup(*db_, "SELECT salt, password_hash, rating FROM users WHERE username = ?");
    lookup.bind(1, username);

    if (lookup.executeStep()) {
        std::string salt = lookup.getColumn(0).getString();
        std::string stored_hash = lookup.getColumn(1).getString();
        int rating = lookup.getColumn(2).getInt();
        if (sha256_hex(salt + password) == stored_hash) {
            return AuthOutcome{true, "", rating, false};
        }
        return AuthOutcome{false, "wrong_password", 0, false};
    }

    // First time we've seen this username -> register it.
    std::string salt = random_salt();
    SQLite::Statement insert(*db_, "INSERT INTO users (username, salt, password_hash, rating) VALUES (?, ?, ?, ?)");
    insert.bind(1, username);
    insert.bind(2, salt);
    insert.bind(3, sha256_hex(salt + password));
    insert.bind(4, kStartingRating);
    insert.exec();
    return AuthOutcome{true, "", kStartingRating, true};
}

std::optional<int> UserRepository::read_rating(const std::string& username) {
    SQLite::Statement query(*db_, "SELECT rating FROM users WHERE username = ?");
    query.bind(1, username);
    if (query.executeStep()) {
        return query.getColumn(0).getInt();
    }
    return std::nullopt;
}

void UserRepository::write_rating(const std::string& username, int rating) {
    SQLite::Statement update(*db_, "UPDATE users SET rating = ? WHERE username = ?");
    update.bind(1, rating);
    update.bind(2, username);
    update.exec();
}

std::optional<int> UserRepository::rating_of(const std::string& username) {
    std::lock_guard<std::mutex> guard(mutex_);
    return read_rating(username);
}

void UserRepository::set_rating(const std::string& username, int rating) {
    std::lock_guard<std::mutex> guard(mutex_);
    write_rating(username, rating);
}

bool UserRepository::rerate_pair(const std::string& first, const std::string& second,
                                 const std::function<std::pair<int, int>(int, int)>& compute) {
    std::lock_guard<std::mutex> guard(mutex_);

    // The lock is what makes this atomic against our own threads. The
    // transaction is what makes the *pair* of writes all-or-nothing against a
    // crash, and against any other connection to the same file: half an ELO
    // exchange on disk would be points created or destroyed, permanently.
    SQLite::Transaction transaction(*db_);

    std::optional<int> before_first = read_rating(first);
    std::optional<int> before_second = read_rating(second);
    if (!before_first.has_value() || !before_second.has_value()) {
        return false;  // rolled back by ~Transaction -- nothing was written yet
    }

    auto [after_first, after_second] = compute(*before_first, *before_second);
    write_rating(first, after_first);
    write_rating(second, after_second);
    transaction.commit();
    return true;
}

bool UserRepository::rerate(const std::string& username, const std::function<int(int)>& compute) {
    std::lock_guard<std::mutex> guard(mutex_);
    SQLite::Transaction transaction(*db_);

    std::optional<int> before = read_rating(username);
    if (!before.has_value()) {
        return false;
    }

    write_rating(username, compute(*before));
    transaction.commit();
    return true;
}

}  // namespace kfc::database
