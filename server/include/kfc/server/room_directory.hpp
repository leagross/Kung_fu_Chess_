#pragma once

#include <optional>
#include <string>

namespace kfc::server {

/// Where a room lives when it may belong to another worker process. Must be thread-safe.
class IRoomDirectory {
public:
    virtual ~IRoomDirectory() = default;

    /// worker_url is the worker's client-facing address, e.g. "ws://localhost:8082".
    virtual void register_room(const std::string& room_name, const std::string& worker_url) = 0;

    /// nullopt if never registered or already forgotten.
    [[nodiscard]] virtual std::optional<std::string> owner_of(const std::string& room_name) = 0;

    /// Not a correctness bug if missed -- implementations also expire entries on their own (TTL).
    virtual void forget_room(const std::string& room_name) = 0;
};

}  // namespace kfc::server
