#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace kfc::database {

/// The account store, as everything above it sees it: authenticate a login,
/// read a rating, and change ratings atomically. Which database is behind it —
/// SQLite today, PostgreSQL at scale — is not visible from here.
///
/// This interface exists because of what `Server_Design.md` concludes: SQLite
/// cannot carry the target load (one writer, one file, one machine), so a
/// second implementation is a matter of when, not whether. Naming the operations
/// separately from the storage means that swap is a new class and a different
/// line in `main`, rather than an edit to every caller.
///
/// It is deliberately **narrow**. There is no `execute_sql`, no cursor, no
/// transaction handle — nothing that would let a caller assume SQL at all. The
/// compound operations below take the arithmetic as a callback precisely so the
/// implementation can hold whatever lock or transaction it needs while the
/// caller supplies only the decision; a `begin()`/`commit()` pair on this
/// interface would leak SQLite's threading model into code that must also work
/// against a sharded Postgres.
///
/// Implementations must be safe to call from many threads at once: the server
/// authenticates on connection threads and applies ratings on match threads.
class IUserStore {
public:
    virtual ~IUserStore() = default;

    struct AuthOutcome {
        bool ok = false;
        std::string reason;             // empty on success; else e.g. "wrong_password"
        int rating = 0;                 // the account's rating, valid when ok
        bool newly_registered = false;  // true if this call created the account
    };

    /// Registers username with password on first sight, or verifies password
    /// against the stored credential on a return visit.
    [[nodiscard]] virtual AuthOutcome authenticate(const std::string& username, const std::string& password) = 0;

    /// The user's current rating, or std::nullopt if there is no such user.
    [[nodiscard]] virtual std::optional<int> rating_of(const std::string& username) = 0;

    /// Re-rates two players as one indivisible step: reads both current
    /// ratings, hands them to compute in that order, and writes back the pair it
    /// returns. False (and nothing written) if either user is unknown.
    ///
    /// compute runs while the store holds whatever it needs to make this
    /// atomic, so it must not call back into the store. It is pure arithmetic
    /// (see elo.hpp) precisely so that it can be.
    [[nodiscard]] virtual bool rerate_pair(const std::string& first, const std::string& second,
                                           const std::function<std::pair<int, int>(int, int)>& compute) = 0;

    /// The one-player form, for an adjustment that depends on the player's
    /// current rating (a forfeit penalty). False if the user is unknown.
    [[nodiscard]] virtual bool rerate(const std::string& username,
                                      const std::function<int(int)>& compute) = 0;
};

}  // namespace kfc::database
