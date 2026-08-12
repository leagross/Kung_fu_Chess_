#pragma once

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace kfc::server {

/// Issues and validates opaque bearer tokens for the HTTP API's protected
/// endpoints -- currently just GET /api/history/{username}, which used to be
/// open to anyone who knew or guessed a username.
///
/// Deliberately separate from SessionRegistry, which tracks the WebSocket
/// game protocol's one-account-one-*connection* invariant: an HTTP client is
/// not a persistent connection, so there is nothing here to release on
/// close. A token simply stays valid once issued.
///
/// **Known limitation, stated rather than hidden**: tokens never expire and
/// are never revoked, and issuing a new one for an account does not
/// invalidate an older one -- multiple tokens for the same account can be
/// valid at once (multiple devices, for instance), and all of them stay
/// valid until the process restarts (this map is in memory only). That is
/// adequate for "a random stranger cannot read your game history by
/// guessing your username," which is the actual gap this closes; a token
/// that must expire or be revocable is a later step, not this one.
///
/// Threading: internally synchronized. HTTP requests arrive on many
/// connection threads at once.
class AuthTokenStore {
public:
    /// A fresh, unguessable token bound to username. Does not check whether
    /// username has an account -- callers issue this only after a successful
    /// register/login, which already established that.
    [[nodiscard]] std::string issue(const std::string& username);

    /// The username token belongs to, or std::nullopt if token was never
    /// issued (including a guessed or malformed one).
    [[nodiscard]] std::optional<std::string> username_for(const std::string& token) const;

private:
    mutable std::mutex mutex_;
    std::unordered_map<std::string, std::string> token_to_username_;
};

}  // namespace kfc::server
