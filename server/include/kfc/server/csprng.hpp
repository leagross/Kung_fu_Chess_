#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>

namespace kfc::server {

/// CSPRNG shared by anything that mints an unguessable identifier (auth tokens, room codes).
/// Not std::mt19937 -- that PRNG's internal state is reconstructible from its own outputs.
/// operator() satisfies UniformRandomBitGenerator, so std::uniform_int_distribution works directly.
class Csprng {
public:
    using result_type = std::uint32_t;
    static constexpr result_type min() { return 0; }
    static constexpr result_type max() { return std::numeric_limits<result_type>::max(); }

    result_type operator()();
    void fill(unsigned char* buffer, std::size_t size);

    /// Process-wide instance; internally synchronized, so callers don't each pay mbedtls's setup cost.
    static Csprng& shared();

    Csprng(const Csprng&) = delete;
    Csprng& operator=(const Csprng&) = delete;

private:
    Csprng();
    ~Csprng();

    struct Impl;
    Impl* impl_;
};

}  // namespace kfc::server
