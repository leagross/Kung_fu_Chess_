#include "kfc/server/auth_token_store.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include <stdexcept>

namespace kfc::server {

namespace {

// A real CSPRNG -- Mbed TLS's CTR_DRBG, seeded from the OS's own entropy via
// mbedtls_entropy_func (the same pairing ixwebsocket's own TLS backend uses
// for its handshakes, see IXSocketMbedTLS.cpp) -- not std::mt19937_64,
// which this used to be. Mersenne Twister is a fast, well-distributed PRNG
// and a perfectly fine choice for a simulation or a game board shuffle, but
// its ~2500-byte internal state is a *known*, practical target: observing a
// few hundred consecutive outputs is enough to reconstruct that state and
// predict every output after it. A 64-hex-character token generated that
// way looks like 256 unguessable bits; it was not one.
//
// One context for the whole process, guarded by its own mutex: CTR_DRBG is
// not safe to call concurrently, and re-seeding per call would be needless
// cost on every register/login.
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

// 32 random bytes (256 bits) as 64 hex characters. Constructed once --
// mbedtls_entropy_init/mbedtls_ctr_drbg_seed do real work (reading OS
// entropy) that every token does not need to repeat -- and thread-safe to
// call from many HTTP connection threads at once via its own mutex, unlike
// the thread_local generator this replaces, which needed one instance per
// thread specifically because std::mt19937_64 was not safe to share.
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
        // Erased here rather than left for some later sweep: the only way
        // an expired entry is ever noticed at all is a caller presenting
        // it, so there is nothing a background pass would clean up sooner
        // that this lookup does not already reach on its own. A token that
        // is issued and never presented again after expiring stays in the
        // map exactly as long as it would have under a sweep running once
        // an hour or once a day -- bounded by real usage, not by a timer.
        token_to_entry_.erase(it);
        return std::nullopt;
    }
    return it->second.username;
}

}  // namespace kfc::server
