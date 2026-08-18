#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace kfc::server {

/// Which usernames currently have a live connection -- one account, one session.
///
/// A second login is refused rather than taking over from the first: the
/// kicked connection's close would arrive later and report a disconnect
/// against a seat the new connection had meanwhile reclaimed.
///
/// Refusing does not block a reconnect: a name is released the moment its
/// connection closes, the same moment the disconnect grace starts, so a
/// player with a countdown running is already free to log in again.
///
/// Dead connections (crash, closed laptop) are caught by WebSocketGameServer's
/// ping interval, which closes the socket and reaches on_close like any other
/// disconnect, releasing the name normally.
///
/// Threading: internally synchronized. Claiming is one locked
/// test-and-insert, so two simultaneous logins for the same name can't both succeed.
class SessionRegistry {
public:
    /// RAII rather than a release() call, so a connection that goes away
    /// along a forgotten path can't leak (and permanently lock) a name.
    class Lease {
    public:
        Lease() = default;
        ~Lease();

        Lease(Lease&& other) noexcept;
        Lease& operator=(Lease&& other) noexcept;
        Lease(const Lease&) = delete;
        Lease& operator=(const Lease&) = delete;

        /// The username this lease holds, or empty if it holds nothing.
        [[nodiscard]] const std::string& username() const { return username_; }

    private:
        friend class SessionRegistry;
        Lease(SessionRegistry& registry, std::string username);

        SessionRegistry* registry_ = nullptr;
        std::string username_;
    };

    /// Claims username for this connection, or std::nullopt if some other live
    /// connection already holds it.
    [[nodiscard]] std::optional<Lease> claim(const std::string& username);

    /// How many usernames are currently connected -- for tests and diagnostics.
    [[nodiscard]] std::size_t live_count() const;

private:
    void release(const std::string& username);

    mutable std::mutex mutex_;
    std::set<std::string> live_;
};

}  // namespace kfc::server
