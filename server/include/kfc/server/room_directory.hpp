#pragma once

#include <optional>
#include <string>

namespace kfc::server {

/// Where a room lives when it may belong to another worker process, not
/// just this one's RoomManager::named_rooms_. Lets RoomManager and its
/// tests stay ignorant of the actual implementation (Redis today).
///
/// Implementations must be safe to call from many threads at once.
class IRoomDirectory {
public:
    virtual ~IRoomDirectory() = default;

    /// worker_url is the worker's client-facing address, e.g. "ws://localhost:8082".
    virtual void register_room(const std::string& room_name, const std::string& worker_url) = 0;

    /// nullopt if never registered or already forgotten -- both mean "not
    /// a room this call can redirect to".
    [[nodiscard]] virtual std::optional<std::string> owner_of(const std::string& room_name) = 0;

    /// Missing this call is not a correctness bug -- implementations are
    /// expected to expire entries on their own too (see RedisRoomDirectory's TTL).
    virtual void forget_room(const std::string& room_name) = 0;
};

}  // namespace kfc::server
