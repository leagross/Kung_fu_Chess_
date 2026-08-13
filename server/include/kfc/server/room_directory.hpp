#pragma once

#include <optional>
#include <string>

namespace kfc::server {

/// Where a room lives, when "this process" is no longer a safe assumption.
///
/// A single kfc_server already knows every room it owns (RoomManager's own
/// named_rooms_ map) -- this interface exists for the question that map
/// cannot answer: a room some *other* worker created. It is deliberately as
/// narrow as kfc::database::IUserStore is for the same reason: naming the
/// three operations a room directory offers, separately from how one is
/// implemented (Redis today; nothing else is expected to exist), is what
/// lets RoomManager and its tests stay ignorant of Redis entirely. A
/// RoomManager built without one (today's single-worker deployment, and
/// every existing test) simply never has a room to look up remotely.
///
/// Implementations must be safe to call from many threads at once: like
/// IUserStore, RoomManager reaches this from many connection threads.
class IRoomDirectory {
public:
    virtual ~IRoomDirectory() = default;

    /// Records that room_name lives on worker_url, the worker's own
    /// client-facing address (e.g. "ws://localhost:8082") -- called once a
    /// room is actually created, never speculatively.
    virtual void register_room(const std::string& room_name, const std::string& worker_url) = 0;

    /// The worker_url a previous register_room recorded for room_name, or
    /// std::nullopt if the directory has never heard of it (never created
    /// anywhere, or already forgotten -- the two look the same from here,
    /// which is fine: both mean "not a room this call can redirect to").
    [[nodiscard]] virtual std::optional<std::string> owner_of(const std::string& room_name) = 0;

    /// Removes room_name's entry -- called once the room is reaped (its last
    /// occupant left). Not calling this promptly is not a correctness bug,
    /// only a stale entry outliving its room; implementations are expected to
    /// expire entries on their own regardless (see RedisRoomDirectory's TTL)
    /// so a missed call here is never permanent.
    virtual void forget_room(const std::string& room_name) = 0;
};

}  // namespace kfc::server
