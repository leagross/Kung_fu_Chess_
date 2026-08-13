#include "kfc/database/user_repository.hpp"


#include <algorithm>
#include <cctype>
#include <format>

#include <SQLiteCpp/Database.h>
#include <SQLiteCpp/Statement.h>
#include <SQLiteCpp/Transaction.h>

#include "kfc/database/elo.hpp"
#include "kfc/database/password_hash.hpp"

namespace kfc::database {

namespace {

// UTC, second precision -- e.g. "2026-08-09T14:03:21Z". Matches what the Java
// api-gateway this replaces serialized java.time.Instant as.
std::string to_iso8601(std::chrono::system_clock::time_point tp) {
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(tp));
}

// Applied only to a username nobody has ever registered -- an existing
// account's username is whatever it already is, and a login attempt against
// it is a password check, not a fresh registration, so these never run on
// that path (see authenticate() below). 3-24 characters keeps a username
// speakable and displayable everywhere the client puts one -- notably
// alongside a room id and a rating on one line -- and alnum-or-underscore
// rules out anything that would need escaping wherever a username is later
// embedded (a log line, a URL path segment, a filename).
bool is_valid_new_username(const std::string& username) {
    constexpr std::size_t kMinLength = 3;
    constexpr std::size_t kMaxLength = 24;
    if (username.size() < kMinLength || username.size() > kMaxLength) {
        return false;
    }
    return std::all_of(username.begin(), username.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

// Same reasoning, for the password half of a fresh registration. The floor
// is a real (if modest) defense against the accounts a script would create
// to test the register endpoint -- 6, not the more commonly quoted 8, is a
// deliberate choice: it is where the floor lands without rejecting
// "hunter2" (7 characters), the password this codebase's own tests already
// used everywhere as their stand-in for "a real one" before this rule
// existed. The ceiling exists so a client cannot feed a multi-megabyte
// string into Argon2 -- memory-hard hashing does not need a long input to
// be expensive, but there is no reason to hash one either.
bool is_valid_new_password(const std::string& password) {
    constexpr std::size_t kMinLength = 6;
    constexpr std::size_t kMaxLength = 128;
    return password.size() >= kMinLength && password.size() <= kMaxLength;
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
    db_->exec(
        "CREATE TABLE IF NOT EXISTS games ("
        "  id              INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  white_username  TEXT NOT NULL,"
        "  black_username  TEXT NOT NULL,"
        "  winner_username TEXT,"
        "  end_reason      TEXT NOT NULL,"
        "  started_at      TEXT NOT NULL,"
        "  ended_at        TEXT NOT NULL"
        ")");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_games_white ON games(white_username)");
    db_->exec("CREATE INDEX IF NOT EXISTS idx_games_black ON games(black_username)");
}

UserRepository::~UserRepository() = default;

UserRepository::AuthOutcome UserRepository::authenticate(const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> guard(mutex_);
    SQLite::Statement lookup(*db_, "SELECT password_hash, rating FROM users WHERE username = ?");
    lookup.bind(1, username);

    if (lookup.executeStep()) {
        std::string stored = lookup.getColumn(0).getString();
        int rating = lookup.getColumn(1).getInt();

        if (!password_hash::verify_password(password, stored)) {
            return AuthOutcome{false, "wrong_password", 0, false};
        }
        return AuthOutcome{true, "", rating, false};
    }

    // First time we've seen this username -> register it, once it and the
    // password clear the new-account rules (see is_valid_new_username/
    // is_valid_new_password's own comments for why these never apply to an
    // existing account). Neither failure creates anything -- a bad username
    // is not implicitly "taken" by having been rejected.
    if (!is_valid_new_username(username)) {
        return AuthOutcome{false, "invalid_username", 0, false};
    }
    if (!is_valid_new_password(password)) {
        return AuthOutcome{false, "weak_password", 0, false};
    }

    // The salt column is left empty: Argon2 carries its own salt inside the
    // encoded string, so there is no longer a second value to keep in step
    // with the hash.
    SQLite::Statement insert(*db_, "INSERT INTO users (username, salt, password_hash, rating) VALUES (?, ?, ?, ?)");
    insert.bind(1, username);
    insert.bind(2, "");
    insert.bind(3, password_hash::hash_password(password));
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

bool UserRepository::user_exists(const std::string& username) {
    std::lock_guard<std::mutex> guard(mutex_);
    SQLite::Statement query(*db_, "SELECT 1 FROM users WHERE username = ? LIMIT 1");
    query.bind(1, username);
    return query.executeStep();
}

void UserRepository::record_game(const std::string& white_username, const std::string& black_username,
                                 std::optional<std::string> winner_username, const std::string& end_reason,
                                 std::chrono::system_clock::time_point started_at,
                                 std::chrono::system_clock::time_point ended_at) {
    std::lock_guard<std::mutex> guard(mutex_);
    SQLite::Statement insert(*db_,
                             "INSERT INTO games (white_username, black_username, winner_username, end_reason, "
                             "started_at, ended_at) VALUES (?, ?, ?, ?, ?, ?)");
    insert.bind(1, white_username);
    insert.bind(2, black_username);
    if (winner_username.has_value()) {
        insert.bind(3, *winner_username);
    } else {
        insert.bind(3);  // NULL -- a draw
    }
    insert.bind(4, end_reason);
    insert.bind(5, to_iso8601(started_at));
    insert.bind(6, to_iso8601(ended_at));
    insert.exec();
}

std::vector<GameRecord> UserRepository::history_for(const std::string& username) {
    std::lock_guard<std::mutex> guard(mutex_);
    SQLite::Statement query(*db_,
                            "SELECT id, white_username, black_username, winner_username, end_reason, "
                            "started_at, ended_at FROM games "
                            "WHERE white_username = ? OR black_username = ? "
                            "ORDER BY ended_at DESC");
    query.bind(1, username);
    query.bind(2, username);

    std::vector<GameRecord> games;
    while (query.executeStep()) {
        GameRecord game;
        game.id = query.getColumn(0).getInt64();
        game.white_username = query.getColumn(1).getString();
        game.black_username = query.getColumn(2).getString();
        if (!query.getColumn(3).isNull()) {
            game.winner_username = query.getColumn(3).getString();
        }
        game.end_reason = query.getColumn(4).getString();
        game.started_at = query.getColumn(5).getString();
        game.ended_at = query.getColumn(6).getString();
        games.push_back(std::move(game));
    }
    return games;
}

}  // namespace kfc::database
