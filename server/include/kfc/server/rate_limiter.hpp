#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kfc::server {

/// Fixed-window request-rate limiter keyed by an arbitrary string (e.g.
/// remote IP). At most 2x max_attempts can land across a window boundary --
/// an acceptable bound for slowing credential stuffing, not an exact rate.
/// now is a parameter so tests can advance time deterministically.
class RateLimiter {
public:
    /// At most max_attempts calls to allow() for the same key succeed within
    /// one window; the rest are refused until it rolls over.
    RateLimiter(int max_attempts, std::chrono::milliseconds window);

    /// Every call counts toward the budget, allowed or not.
    [[nodiscard]] bool allow(const std::string& key, std::chrono::steady_clock::time_point now);

    [[nodiscard]] std::size_t bucket_count() const;

private:
    struct Bucket {
        int count = 0;
        std::chrono::steady_clock::time_point window_start;
    };

    // Called periodically from allow(), not every call, so eviction is
    // amortized rather than an O(n) scan on the busiest path. Without this,
    // buckets_ only grows, one entry per distinct caller forever.
    void evict_expired(std::chrono::steady_clock::time_point now);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
    int max_attempts_;
    std::chrono::milliseconds window_;
    static constexpr int kSweepEveryNCalls = 1000;
    int calls_since_sweep_ = 0;
};

}  // namespace kfc::server
