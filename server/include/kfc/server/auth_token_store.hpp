#pragma once

#include <chrono>
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
/// close. A token simply stays valid until it expires.
///
/// Tokens expire kTokenLifetime after being issued (checked lazily, on the
/// next username_for() call that happens to land on an expired one -- see
/// its own comment for why that is enough and no background sweep is
/// needed) and are never explicitly revoked before that; issuing a new one
/// for an account does not invalidate an older one, so multiple tokens for
/// the same account can be valid at once (multiple devices, for instance).
/// All of them stay valid only in this process's memory -- a restart clears
/// every token, which simply means every client re-authenticates, the same
/// as an expiry would have made them do anyway.
///
/// **Known limitation, stated rather than hidden**: there is still no way
/// to revoke a token *before* it expires (a "log out everywhere" button, or
/// reacting to a leaked token) -- that needs an endpoint this API doesn't
/// have yet, not a gap in this class's own bookkeeping.
///
/// Threading: internally synchronized. HTTP requests arrive on many
/// connection threads at once.
class AuthTokenStore {
public:
    /// How long after issue() a token remains valid. A day is generous
    /// enough that a real player reloading the history page hours later is
    /// never surprised by a 401, while still being a real bound -- not
    /// "forever," which is what this was before.
    static constexpr std::chrono::seconds kTokenLifetime{24 * 60 * 60};

    /// A fresh, unguessable token bound to username, valid until
    /// kTokenLifetime after now. now is a parameter rather than read from
    /// the clock internally, the same choice RateLimiter::allow makes, so
    /// tests can move time forward deterministically instead of waiting out
    /// a real day; every real caller simply omits it. Does not check
    /// whether username has an account -- callers issue this only after a
    /// successful register/login, which already established that.
    [[nodiscard]] std::string issue(const std::string& username,
                                    std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

    /// The username token belongs to, or std::nullopt if token was never
    /// issued (including a guessed or malformed one) or has expired as of
    /// now. An expired token found here is also erased, which is this
    /// class's only cleanup -- see the class comment on why a background
    /// sweep buys nothing a real deployment would notice.
    [[nodiscard]] std::optional<std::string> username_for(
        const std::string& token, std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now());

private:
    struct Entry {
        std::string username;
        std::chrono::steady_clock::time_point expires_at;
    };

    mutable std::mutex mutex_;
    std::unordered_map<std::string, Entry> token_to_entry_;
};

}  // namespace kfc::server
