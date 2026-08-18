#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace kfc::server {

/// Issues and validates opaque bearer tokens for the HTTP API's protected
/// endpoints. Expiry is checked lazily on username_for(); there is no
/// background sweep and no revocation before expiry.
///
/// Threading: internally synchronized.
class AuthTokenStore {
public:
    static constexpr std::chrono::seconds kTokenLifetime{24 * 60 * 60};

    /// now is a parameter (not read from the clock) so tests can advance
    /// time deterministically.
    [[nodiscard]] std::string issue(const std::string& username,
                                    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// Returns nullopt if token is unknown or expired; an expired entry
    /// found here is erased as a side effect.
    [[nodiscard]] std::optional<std::string> username_for(
        const std::string& token, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

private:
    struct Entry {
        std::string username;
        std::chrono::steady_clock::time_point expires_at;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> token_to_entry_;
};

}  // namespace kfc::server
