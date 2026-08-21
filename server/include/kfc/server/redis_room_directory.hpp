#pragma once

#include <memory>
#include <mutex>
#include <string>

#include "kfc/server/room_directory.hpp"

struct redisContext;

namespace kfc::server {

/// The IRoomDirectory this repo runs: a thin hiredis wrapper storing
/// "room:<name>" -> worker_url, with a TTL as a safety net against a crash
/// before forget_room's cleanup runs. Every call holds mutex_ for the whole
/// round trip (hiredis's redisContext isn't safe for concurrent use).
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
