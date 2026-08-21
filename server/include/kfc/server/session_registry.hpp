#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace kfc::server {

/// Which usernames have a live connection -- one account, one session. A
/// name releases the moment its connection closes (crash/timeout included),
/// so a reconnect is never blocked. Internally synchronized.
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
