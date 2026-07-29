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
/// **Known limitation: a connection that vanishes without a closing handshake.**
/// A crash, a closed laptop or lost wi-fi leaves the server holding a socket it
/// does not yet know is dead, so the name stays claimed until TCP eventually
/// gives up -- and the player is told "already_logged_in" if they come back
/// before then.
///
/// This is a symptom, not the disease. The same missing dead-peer detection
/// already breaks the 20-second grace itself: nothing starts the countdown
/// either, so the opponent sees no timer and the seat is never made reclaimable.
/// Before this class existed the returning player was silently seated as a
/// *spectator of their own game*, which is the same failure with no explanation
/// attached. The refusal at least says what happened.
///
/// The real fix is server-side ping, and it is not available here: IXWebSocket
/// 11.4.5's WebSocketTransport sets `_pongReceived = false` when a connection
/// opens and then closes any connection whose first ping interval elapses with
/// that flag still false -- before it has ever sent a ping. Enabling
/// setPingInterval therefore disconnects every healthy client after exactly one
/// interval, which was measured, not guessed. Fixing this needs a newer
/// IXWebSocket or an application-level heartbeat in our own protocol.
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
