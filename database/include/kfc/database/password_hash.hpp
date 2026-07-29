#pragma once

#include <string>

namespace kfc::database {

/// Turning a password into something safe to store, and checking one against it.
///
/// **Why not a plain salted SHA-256, which is what this used to be.** SHA-256 is
/// built to be *fast* -- that is its job everywhere else. A password hash needs
/// the opposite. Commodity hardware computes billions of SHA-256 per second, so
/// a stolen database of salted SHA-256 hashes is a dictionary attack that
/// finishes over a weekend; the salt only forces the attacker to work one
/// account at a time, it does not make the work hard.
///
/// Argon2id makes each guess cost measurable **time and memory**, and memory is
/// the part that matters: it is what a GPU or an ASIC cannot simply add
/// thousands of in parallel. The `id` variant is the one to use by default --
/// it takes Argon2i's resistance to side-channel attacks on the first pass and
/// Argon2d's resistance to time-memory trade-offs on the rest.
///
/// Verification is constant-time (Argon2's own comparison), so the check cannot
/// leak how much of a guess was right through how long it took.
namespace password_hash {

/// The cost parameters, at OWASP's recommended baseline for Argon2id.
///
/// These are stored *inside* every hash this produces, so raising them later
/// does not invalidate existing accounts: an old hash keeps verifying with the
/// parameters it was made with, and is rewritten at the new cost the next time
/// its owner logs in successfully.
inline constexpr unsigned int kMemoryKiB = 19456;  // 19 MiB per hash
inline constexpr unsigned int kIterations = 2;
inline constexpr unsigned int kParallelism = 1;  // see ARGON2_NO_THREADS in CMakeLists

/// Hashes password with a fresh random salt, returning the PHC string that
/// should be stored -- `$argon2id$v=19$m=...,t=...,p=1$<salt>$<hash>`.
///
/// The salt is generated per call and carried inside the result, so there is no
/// second column to keep in step with it and no way to store a hash without its
/// salt. Throws std::runtime_error if hashing fails.
[[nodiscard]] std::string hash_password(const std::string& password);

/// True when password matches the stored PHC string. False for a wrong
/// password *and* for a malformed or unrecognised `stored` -- a corrupt
/// credential must never authenticate anyone.
[[nodiscard]] bool verify_password(const std::string& password, const std::string& stored);

/// Whether stored was produced by hash_password rather than being one of the
/// old salted-SHA-256 credentials. Lets the account store keep verifying
/// accounts created before this existed, and quietly upgrade each one the next
/// time its owner proves they know the password -- so nobody is locked out and
/// nothing has to be migrated in a batch.
[[nodiscard]] bool is_argon2(const std::string& stored);

}  // namespace password_hash

}  // namespace kfc::database
