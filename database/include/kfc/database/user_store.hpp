#pragma once

#include <functional>
#include <optional>
#include <string>
#include <utility>

namespace kfc::database {

/// Account store contract: compound operations take the arithmetic as a callback so implementations
/// can hold their own lock/transaction instead of exposing a cursor. Must be thread-safe.
class IUserStore {
public:
    virtual ~IUserStore() = default;

    struct AuthOutcome {
        bool ok = false;
        std::string reason;             // empty on success; else e.g. "wrong_password"
        int rating = 0;
        bool newly_registered = false;
    };

    /// Registers username on first sight, else verifies password against the stored credential.
    [[nodiscard]] virtual AuthOutcome authenticate(const std::string& username, const std::string& password) = 0;

    [[nodiscard]] virtual std::optional<int> rating_of(const std::string& username) = 0;

    /// Reads both ratings, writes back what compute returns, atomically. False if either user is unknown.
    [[nodiscard]] virtual bool rerate_pair(const std::string& first, const std::string& second,
                                           const std::function<std::pair<int, int>(int, int)>& compute) = 0;

    /// One-player form of rerate_pair, for a forfeit penalty.
    [[nodiscard]] virtual bool rerate(const std::string& username,
                                      const std::function<int(int)>& compute) = 0;
};

}  // namespace kfc::database
