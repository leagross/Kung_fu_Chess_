#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace kfc::database {

/// The account store, as everything above it sees it: authenticate a login,
/// read a rating, and change ratings atomically. The backing database (SQLite
/// today) is not visible from here, so swapping it is a new class, not an
/// edit to every caller.
///
/// Deliberately narrow: no `execute_sql`, no cursor, no transaction handle.
/// The compound operations take the arithmetic as a callback so the
/// implementation can hold whatever lock/transaction it needs internally.
///
/// Implementations must be safe to call from many threads at once.
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

    /// Reads both current ratings, hands them to compute, writes back the
    /// pair it returns, all as one atomic step. False if either user is
    /// unknown. compute must not call back into the store.
    [[nodiscard]] virtual bool rerate_pair(const std::string& first, const std::string& second,
                                           const std::function<std::pair<int, int>(int, int)>& compute) = 0;

    /// One-player form of rerate_pair, for a forfeit penalty. False if unknown.
    [[nodiscard]] virtual bool rerate(const std::string& username,
                                      const std::function<int(int)>& compute) = 0;
};

}  // namespace kfc::database
