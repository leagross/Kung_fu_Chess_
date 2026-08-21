#include "kfc/server/rate_limiter.hpp"

namespace kfc::server {

RateLimiter::RateLimiter(int max_attempts, std::chrono::milliseconds window)
    : max_attempts_(max_attempts), window_(window) {}

bool RateLimiter::allow(const std::string& key, std::chrono::steady_clock::time_point now) {
    std::lock_guard<std::mutex> guard(mutex_);

    if (++calls_since_sweep_ >= kSweepEveryNCalls) {
        calls_since_sweep_ = 0;
        evict_expired(now);
    }

    Bucket& bucket = buckets_[key];  // default-constructed (count 0, window_start the epoch) if new

    if (now - bucket.window_start >= window_) {
        bucket.count = 0;
        bucket.window_start = now;
    }

    ++bucket.count;
    return bucket.count <= max_attempts_;
}

std::size_t RateLimiter::bucket_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return buckets_.size();
}

void RateLimiter::evict_expired(std::chrono::steady_clock::time_point now) {
    // Erasing an expired bucket only reclaims memory; the next allow() would
    // reset it from scratch anyway.
    for (auto it = buckets_.begin(); it != buckets_.end();) {
        if (now - it->second.window_start >= window_) {
            it = buckets_.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace kfc::server
