#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <set>
#include <string>

namespace kfc::server {

/// Which usernames currently have a live connection -- one account, one session.
///
/// Without this the same account could be logged in twice at once: two seats in
/// two different games, two rating changes landing for one player, and a room
/// its own creator could join as the opponent. Worse, a returning player could
/// not be told apart from a second copy of themselves, which is the one
/// distinction Match::reclaimable_seat_for exists to make.
///
/// **Why a second login is refused rather than taking over from the first.**
/// Taking over is the other reasonable policy, and it is the wrong one here: the
/// kicked connection's close arrives later, asynchronously, and would report a
/// disconnect against a seat the new connection had meanwhile reclaimed --
/// freezing, and eventually forfeiting, a game that is being played perfectly
/// normally by its rightful owner.
///
/// **Refusing does not block a reconnect**, because of one invariant: a name is
/// held for exactly as long as a live connection holds it, and is released the
/// moment that connection closes -- which is the same moment the disconnect
/// grace starts. So if a player has a countdown running, their old session is by
/// definition already gone from here, and their name is free to log in with
/// again. The two mechanisms cannot fight.
///
/// **A connection that vanishes without a closing handshake** (a crash, a
/// closed laptop, lost wi-fi) used to leave the server holding a socket it
/// did not know was dead, so the name stayed claimed until TCP eventually gave
/// up, and a returning player was told "already_logged_in". That was a
/// symptom of the same missing dead-peer detection that also broke the
/// 20-second disconnect grace: nothing started the countdown either, so the
/// opponent saw no timer and the seat was never made reclaimable.
///
/// **This is fixed now.** `WebSocketGameServer` calls `setPingInterval` on
/// every connection (see kIdlePingIntervalSecs's own doc comment), so
/// IXWebSocket itself closes a socket that stops answering pings, which
/// reaches `on_close` exactly like any other disconnect -- the name here is
/// released and the 20-second grace starts normally. This doc used to say
/// IXWebSocket 11.4.5 disconnected every *healthy* client after one interval
/// regardless of whether it answered; that was true when this class was
/// written but no longer reflects the code -- confirmed by an idle,
/// healthy connection surviving multiple ping intervals in practice. Verify
/// against the current ping wiring before trusting this paragraph, the same
/// way its predecessor should have been re-checked.
///
/// Threading: internally synchronized. Logins arrive on many IXWebSocket
/// connection threads at once, and two simultaneous logins for the same name
/// must not both succeed -- which is why claiming is one locked test-and-insert
/// rather than a separate "is it taken?" and "take it".
class SessionRegistry {
public:
    /// Holds one username for as long as it exists, and gives it back when
    /// destroyed. RAII rather than a matching release() call, so a name cannot
    /// be leaked by a connection that goes away along a path someone forgot --
    /// a leaked name would lock its owner out permanently, with no way to
    /// recover short of restarting the server.
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
