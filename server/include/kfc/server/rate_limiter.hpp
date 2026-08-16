#pragma once

#include <chrono>
#include <cstddef>
#include <mutex>
#include <string>
#include <unordered_map>

namespace kfc::server {

/// A fixed-window request-rate limiter, keyed by an arbitrary string (the
/// caller's remote IP, for the HTTP API's register/login endpoints -- the one
/// place on this server that has no rate limiting at all yet, unlike the
/// WebSocket game protocol's own kMaxMessagesPerSecond in ClientSession).
///
/// Without this, a script can hammer /api/auth/login trying passwords for a
/// known username as fast as the network allows, or hammer /api/auth/register
/// creating accounts, and nothing here even slows it down.
///
/// **Why fixed windows, not a sliding log or a token bucket.** A fixed window
/// is the simplest thing that is still a real deterrent: at most 2x
/// max_attempts can land in any rolling period that straddles a window
/// boundary, which is a perfectly acceptable bound for "slow down credential
/// stuffing," not a promise of an exact rate. A sliding window or token
/// bucket would be a more precise limiter for a fraction of the code's
/// simplicity -- not worth it here.
///
/// now is taken as a parameter rather than read from the clock internally,
/// the same choice Match::tick and DisconnectWatch::advance make, so tests
/// can move time forward deterministically instead of sleeping for real.
///
/// Threading: internally synchronized. HTTP requests arrive on many
/// connection threads at once.
class RateLimiter {
public:
    /// At most max_attempts calls to allow() for the same key succeed within
    /// any one window; the rest are refused until the window rolls over.
    RateLimiter(int max_attempts, std::chrono::milliseconds window);

    /// True if this call counts as one of key's max_attempts for the window
    /// containing now; false if key has already used up its budget for that
    /// window. Every call counts, allowed or not -- a caller that keeps
    /// retrying does not get to reset its own window early.
    [[nodiscard]] bool allow(const std::string& key, std::chrono::steady_clock::time_point now);

    /// How many distinct keys currently hold a bucket. For tests and
    /// diagnostics only -- allow()'s own behaviour never depends on this.
    [[nodiscard]] std::size_t bucket_count() const;

private:
    struct Bucket {
        int count = 0;
        std::chrono::steady_clock::time_point window_start;
    };

    // Sweeps buckets_ for entries whose window has already elapsed -- called
    // periodically from allow() rather than on every call, so eviction is
    // amortized instead of paying an O(n) scan on the server's busiest path.
    // Long-running server, ever-growing set of distinct remote IPs, no other
    // place a key is ever removed -- without this buckets_ only ever grows,
    // one entry per distinct caller for the lifetime of the process.
    void evict_expired(std::chrono::steady_clock::time_point now);

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Bucket> buckets_;
    int max_attempts_;
    std::chrono::milliseconds window_;
    // Sweep every this many allow() calls, not this many seconds -- an idle
    // server (no calls at all) has nothing to evict, so there is no need for
    // a wall-clock timer or a background thread for this.
    static constexpr int kSweepEveryNCalls = 1000;
    int calls_since_sweep_ = 0;
};

}  // namespace kfc::server
