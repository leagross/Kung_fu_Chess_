#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "kfc/server/room_directory.hpp"

struct redisContext;

namespace kfc::server {

/// The IRoomDirectory this repo runs: a thin wrapper over hiredis's
/// synchronous API, storing "room:<name>" -> worker_url as a Redis string.
///
/// Entries carry a TTL as a safety net: forget_room is the normal cleanup
/// path, but a crash between a room being reaped and forget_room running
/// would otherwise leak the entry forever.
///
/// Threading: hiredis's synchronous redisContext is not safe for concurrent
/// use, so every call here holds mutex_ for the whole round trip.
class RedisRoomDirectory : public IRoomDirectory {
public:
    /// Six hours: an order of magnitude past any plausible single game.
    static constexpr int kEntryTtlSeconds = 6 * 60 * 60;

    /// Throws std::runtime_error if the connection cannot be established.
    RedisRoomDirectory(const std::string& host, int port);
    ~RedisRoomDirectory() override;

    RedisRoomDirectory(const RedisRoomDirectory&) = delete;
    RedisRoomDirectory& operator=(const RedisRoomDirectory&) = delete;

    void register_room(const std::string& room_name, const std::string& worker_url) override;
    [[nodiscard]] std::optional<std::string> owner_of(const std::string& room_name) override;
    void forget_room(const std::string& room_name) override;

private:
    struct ContextDeleter {
        void operator()(redisContext* context) const;
    };

    std::mutex mutex_;
    std::unique_ptr<redisContext, ContextDeleter> context_;
};

}  // namespace kfc::server
