#include "kfc/server/rate_limiter.hpp"

namespace kfc::server {

RateLimiter::RateLimiter(int max_attempts, std::chrono::milliseconds window)
    : max_attempts_(max_attempts), window_(window) {}

bool RateLimiter::allow(const std::string& key, std::chrono::steady_clock::time_point now) {
    std::lock_guard<std::mutex> guard(mutex_);
    Bucket& bucket = buckets_[key];  // default-constructed (count 0, window_start the epoch) if new

    if (now - bucket.window_start >= window_) {
        bucket.count = 0;
        bucket.window_start = now;
    }

    ++bucket.count;
    return bucket.count <= max_attempts_;
}

}  // namespace kfc::server
