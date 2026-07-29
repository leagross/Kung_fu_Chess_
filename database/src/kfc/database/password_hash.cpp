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

// 16 bytes, the size Argon2's own documentation recommends. A salt does not
// have to be secret, only unique -- it exists so that two accounts with the
// same password do not share a hash, and so one precomputed table cannot cover
// every account at once.
constexpr std::size_t kSaltBytes = 16;

// 32 bytes of output: the same length as SHA-256's, and far beyond what a
// collision would need to be a problem here.
constexpr std::size_t kHashBytes = 32;

// std::random_device, not the mt19937 the old salt generator used. mt19937 is
// deterministic given its seed and its whole state can be recovered from its
// output -- fine for choosing a room id, not for anything a password's
// uniqueness rests on.
std::vector<std::uint8_t> random_salt() {
    std::random_device source;
    std::vector<std::uint8_t> salt(kSaltBytes);
    for (std::uint8_t& byte : salt) {
        byte = static_cast<std::uint8_t>(source() & 0xFF);
    }
    return salt;
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
        // Refusing loudly rather than falling back to something weaker: a
        // caller that got a string back is entitled to assume it is an Argon2
        // hash, and silently storing anything else would be worse than not
        // registering the account at all.
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
    // Argon2's own verify: it re-derives with the parameters and salt read out
    // of `stored`, and compares in constant time, so a wrong guess cannot be
    // narrowed down by how long the answer took.
    return argon2id_verify(stored.c_str(), password.data(), password.size()) == ARGON2_OK;
}

bool is_argon2(const std::string& stored) {
    return stored.rfind("$argon2id$", 0) == 0;
}

}  // namespace password_hash
}  // namespace kfc::database
