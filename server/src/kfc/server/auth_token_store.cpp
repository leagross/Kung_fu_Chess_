#include "kfc/server/auth_token_store.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include <stdexcept>

namespace kfc::server {

namespace {

// CSPRNG (Mbed TLS CTR_DRBG) for unguessable tokens -- not std::mt19937_64,
// whose internal state can be reconstructed from a few hundred outputs.
// One process-wide context guarded by a mutex; CTR_DRBG isn't safe to call
// concurrently and re-seeding per call would be wasteful.
class TokenRandom {
public:
    TokenRandom() {
        mbedtls_entropy_init(&entropy_);
        mbedtls_ctr_drbg_init(&ctr_drbg_);
        static constexpr char kPersonalization[] = "kfc_auth_token";
        int rc = mbedtls_ctr_drbg_seed(&ctr_drbg_, mbedtls_entropy_func, &entropy_,
                                       reinterpret_cast<const unsigned char*>(kPersonalization),
                                       sizeof(kPersonalization) - 1);
        if (rc != 0) {
            throw std::runtime_error("mbedtls_ctr_drbg_seed failed");
        }
    }

    ~TokenRandom() {
        mbedtls_ctr_drbg_free(&ctr_drbg_);
        mbedtls_entropy_free(&entropy_);
    }

    TokenRandom(const TokenRandom&) = delete;
    TokenRandom& operator=(const TokenRandom&) = delete;

    void fill(unsigned char* buffer, std::size_t size) {
        std::lock_guard<std::mutex> guard(mutex_);
        if (mbedtls_ctr_drbg_random(&ctr_drbg_, buffer, size) != 0) {
            throw std::runtime_error("mbedtls_ctr_drbg_random failed");
        }
    }

private:
    std::mutex mutex_;
    mbedtls_entropy_context entropy_;
    mbedtls_ctr_drbg_context ctr_drbg_;
};

// 32 random bytes (256 bits) as 64 hex characters.
std::string random_hex_token() {
    static constexpr char kHexDigits[] = "0123456789abcdef";
    static TokenRandom random;

    unsigned char raw[32];
    random.fill(raw, sizeof(raw));

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
    return token;
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
