#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace kfc::server {

/// Issues and validates opaque bearer tokens for the HTTP API's protected
/// endpoints. Expiry is checked lazily on username_for(); issue() also sweeps
/// every kSweepEveryNCalls calls so a token nobody ever rechecks still gets
/// reclaimed. No revocation before expiry. Internally synchronized.
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

    /// Live entry count, expired or not -- for tests and /metrics, not part of the auth contract.
    [[nodiscard]] std::size_t token_count() const;

private:
    struct Entry {
        std::string username;
        std::chrono::steady_clock::time_point expires_at;
    };

    // Caller must already hold mutex_.
    void evict_expired(std::chrono::steady_clock::time_point now);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> token_to_entry_;
    static constexpr int kSweepEveryNCalls = 500;
    int calls_since_sweep_ = 0;
};

}  // namespace kfc::server
