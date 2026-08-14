#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "kfc/server/room_directory.hpp"

struct redisContext;

namespace kfc::server {

/// The IRoomDirectory this repo actually runs -- a thin wrapper over
/// hiredis's synchronous API, storing "room:<name>" -> worker_url as a plain
/// Redis string. Only three commands are ever issued (SET/GET/DEL), which is
/// the whole reason IRoomDirectory stays this narrow.
///
/// Entries carry a TTL (kEntryTtlSeconds) as a self-healing safety net:
/// forget_room is the normal cleanup path, but a crash between a room being
/// reaped and forget_room running would otherwise leak the entry forever.
/// register_room refreshes the TTL every time (see Match/RoomManager --
/// nothing currently re-registers a long-lived room, so the TTL is set
/// generously rather than assumed to be renewed).
///
/// Threading: hiredis's synchronous redisContext is not safe for concurrent
/// use from multiple threads, and RoomManager (like UserRepository) is
/// reached from many connection threads at once -- so every call here takes
/// mutex_ for the whole round trip, the same choice UserRepository makes
/// around its own single SQLite connection.
class RedisRoomDirectory : public IRoomDirectory {
public:
    /// How long a registered room stays in Redis without being refreshed or
    /// explicitly forgotten -- long enough that no real game could still be
    /// running, short enough that a missed forget_room is not permanent. Six
    /// hours: an order of magnitude past any plausible single game.
    static constexpr int kEntryTtlSeconds = 6 * 60 * 60;

    /// Connects to a Redis server at host:port. Throws std::runtime_error if
    /// the connection cannot be established -- a worker that cannot reach its
    /// own room directory should fail loudly at startup, not silently run
    /// unable to route anything, the same posture UserRepository takes for a
    /// database it can't open.
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
