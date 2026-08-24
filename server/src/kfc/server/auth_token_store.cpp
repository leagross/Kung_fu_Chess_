#include "kfc/server/auth_token_store.hpp"

#include <iterator>

#include "kfc/server/csprng.hpp"

namespace kfc::server {

namespace {

// 32 random bytes (256 bits) as 64 hex characters.
std::string random_hex_token() {
    static constexpr char kHexDigits[] = "0123456789abcdef";

    unsigned char raw[32];
    Csprng::shared().fill(raw, sizeof(raw));

    std::string token(64, '0');
    for (std::size_t i = 0; i < sizeof(raw); ++i) {
        token[i * 2] = kHexDigits[(raw[i] >> 4) & 0x0F];
        token[i * 2 + 1] = kHexDigits[raw[i] & 0x0F];
    }
    return token;
}

}  // namespace

std::string AuthTokenStore::issue(const std::string& username, std::chrono::steady_clock::time_point now) {
    std::string token = random_hex_token();
    Entry entry{username, now + kTokenLifetime};
    std::lock_guard<std::mutex> guard(mutex_);
    token_to_entry_[token] = std::move(entry);
    if (++calls_since_sweep_ >= kSweepEveryNCalls) {
        calls_since_sweep_ = 0;
        evict_expired(now);
    }
    return token;
}

// Caller already holds mutex_. Amortizes cleanup instead of a background thread, same pattern
// RateLimiter uses -- an issued-but-never-checked-again token would otherwise sit forever.
void AuthTokenStore::evict_expired(std::chrono::steady_clock::time_point now) {
    for (auto it = token_to_entry_.begin(); it != token_to_entry_.end();) {
        it = now >= it->second.expires_at ? token_to_entry_.erase(it) : std::next(it);
    }
}

std::size_t AuthTokenStore::token_count() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return token_to_entry_.size();
}

std::optional<std::string> AuthTokenStore::username_for(const std::string& token,
                                                         std::chrono::steady_clock::time_point now) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = token_to_entry_.find(token);
    if (it == token_to_entry_.end()) {
        return std::nullopt;
    }
    if (now >= it->second.expires_at) {
        token_to_entry_.erase(it);
        return std::nullopt;
    }
    return it->second.username;
}

}  // namespace kfc::server
