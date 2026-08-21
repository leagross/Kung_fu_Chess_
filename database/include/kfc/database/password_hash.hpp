#pragma once

#include <string>

namespace kfc::database {

/// Password hashing via Argon2id, chosen over salted SHA-256 because it costs
/// attackers real time and memory per guess rather than just per-account salt.
namespace password_hash {

/// OWASP-recommended baseline cost for Argon2id. Stored inside each hash, so
/// raising these later doesn't invalidate existing accounts.
inline constexpr unsigned int kMemoryKiB = 19456;  // 19 MiB per hash
inline constexpr unsigned int kIterations = 2;
inline constexpr unsigned int kParallelism = 1;  // see ARGON2_NO_THREADS in CMakeLists

/// Hashes password with a fresh random salt, returning the PHC string to store.
/// Throws std::runtime_error if hashing fails.
[[nodiscard]] std::string hash_password(const std::string& password);

/// True when password matches the stored PHC string. False for a wrong
/// password or a malformed/unrecognised `stored`.
[[nodiscard]] bool verify_password(const std::string& password, const std::string& stored);

}  // namespace password_hash

}  // namespace kfc::database
