#include "kfc/database/password_hash.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include <argon2.h>

namespace kfc::database {
namespace password_hash {

namespace {

constexpr std::size_t kSaltBytes = 16;
constexpr std::size_t kHashBytes = 32;

// random_device, not a PRNG like mt19937: salt uniqueness must not be
// predictable from observed output.
std::vector<std::uint8_t> random_salt() {
    std::random_device source;
    std::vector<std::uint8_t> salt(kSaltBytes);
    for (std::uint8_t& byte : salt) {
        byte = static_cast<std::uint8_t>(source() & 0xFF);
    }
    return salt;
}

bool is_argon2(const std::string& stored) {
    return stored.rfind("$argon2id$", 0) == 0;
}

}  // namespace

std::string hash_password(const std::string& password) {
    std::vector<std::uint8_t> salt = random_salt();

    std::size_t encoded_length =
        argon2_encodedlen(kIterations, kMemoryKiB, kParallelism, static_cast<std::uint32_t>(salt.size()),
                          static_cast<std::uint32_t>(kHashBytes), Argon2_id);
    std::string encoded(encoded_length, '\0');

    int result = argon2id_hash_encoded(kIterations, kMemoryKiB, kParallelism, password.data(), password.size(),
                                       salt.data(), salt.size(), kHashBytes, encoded.data(), encoded.size());
    if (result != ARGON2_OK) {
        throw std::runtime_error(std::string("Argon2 hashing failed: ") + argon2_error_message(result));
    }

    // argon2_encodedlen counts the terminating NUL; the std::string must not.
    encoded.resize(encoded.find('\0') == std::string::npos ? encoded.size() : encoded.find('\0'));
    return encoded;
}

bool verify_password(const std::string& password, const std::string& stored) {
    if (!is_argon2(stored)) {
        return false;
    }
    return argon2id_verify(stored.c_str(), password.data(), password.size()) == ARGON2_OK;
}

}  // namespace password_hash
}  // namespace kfc::database
