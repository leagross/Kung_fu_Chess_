#include "kfc/server/csprng.hpp"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>

#include <mutex>
#include <stdexcept>

namespace kfc::server {

struct Csprng::Impl {
    std::mutex mutex;
    mbedtls_entropy_context entropy;
    mbedtls_ctr_drbg_context ctr_drbg;
};

Csprng::Csprng() : impl_(new Impl()) {
    mbedtls_entropy_init(&impl_->entropy);
    mbedtls_ctr_drbg_init(&impl_->ctr_drbg);
    static constexpr char kPersonalization[] = "kfc_csprng";
    int rc = mbedtls_ctr_drbg_seed(&impl_->ctr_drbg, mbedtls_entropy_func, &impl_->entropy,
                                   reinterpret_cast<const unsigned char*>(kPersonalization),
                                   sizeof(kPersonalization) - 1);
    if (rc != 0) {
        throw std::runtime_error("mbedtls_ctr_drbg_seed failed");
    }
}

Csprng::~Csprng() {
    mbedtls_ctr_drbg_free(&impl_->ctr_drbg);
    mbedtls_entropy_free(&impl_->entropy);
    delete impl_;
}

Csprng& Csprng::shared() {
    static Csprng instance;
    return instance;
}

void Csprng::fill(unsigned char* buffer, std::size_t size) {
    std::lock_guard<std::mutex> guard(impl_->mutex);
    if (mbedtls_ctr_drbg_random(&impl_->ctr_drbg, buffer, size) != 0) {
        throw std::runtime_error("mbedtls_ctr_drbg_random failed");
    }
}

Csprng::result_type Csprng::operator()() {
    result_type value = 0;
    fill(reinterpret_cast<unsigned char*>(&value), sizeof(value));
    return value;
}

}  // namespace kfc::server
