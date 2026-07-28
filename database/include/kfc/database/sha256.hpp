#pragma once

#include <string>

namespace kfc::database {

/// SHA-256 of `input`, returned as a 64-character lowercase hex string.
/// Self-contained (no OpenSSL -- this project builds with TLS off) so the
/// server can hash a salted password before storing it, never keeping the
/// plaintext. Not a password-stretching KDF like bcrypt/argon2; a deliberate
/// scope choice for a local, TLS-off educational server (see UserRepository).
[[nodiscard]] std::string sha256_hex(const std::string& input);

}  // namespace kfc::database
