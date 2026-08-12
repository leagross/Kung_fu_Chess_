#include "kfc/server/auth_token_store.hpp"

#include <random>

namespace kfc::server {

namespace {

// 32 random bytes as 64 hex characters -- 256 bits, far past anything worth
// brute-forcing over a network. thread_local so concurrent issue() calls from
// different HTTP connection threads never share generator state (which
// std::mt19937 is not safe for); seeded once per thread from random_device,
// the same non-reproducible-across-runs source generate_room_id() uses, for
// the same reason: a token that restarted from a predictable sequence would
// not be a token.
std::string random_hex_token() {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    thread_local std::mt19937_64 generator(std::random_device{}());
    std::uniform_int_distribution<int> pick(0, 15);

    std::string token(64, '0');
    for (char& c : token) {
        c = kHexDigits[pick(generator)];
    }
    return token;
}

}  // namespace

std::string AuthTokenStore::issue(const std::string& username) {
    std::string token = random_hex_token();
    std::lock_guard<std::mutex> guard(mutex_);
    token_to_username_[token] = username;
    return token;
}

std::optional<std::string> AuthTokenStore::username_for(const std::string& token) const {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = token_to_username_.find(token);
    if (it == token_to_username_.end()) {
        return std::nullopt;
    }
    return it->second;
}

}  // namespace kfc::server
