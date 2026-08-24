#pragma once

#include <string>

namespace kfc::database {

/// Argon2id password hashing: costs attackers time and memory per guess, unlike salted SHA-256.
namespace password_hash {

/// OWASP-recommended baseline cost; stored inside each hash so raising it won't invalidate old accounts.
inline constexpr unsigned int kMemoryKiB = 19456;  // 19 MiB per hash
inline constexpr unsigned int kIterations = 2;
inline constexpr unsigned int kParallelism = 1;  // see ARGON2_NO_THREADS in CMakeLists

/// Returns the PHC string to store. Throws std::runtime_error if hashing fails.
[[nodiscard]] std::string hash_password(const std::string& password);

/// False for a wrong password or a malformed/unrecognised `stored`.
[[nodiscard]] bool verify_password(const std::string& password, const std::string& stored);

}  // namespace password_hash

}  // namespace kfc::database
