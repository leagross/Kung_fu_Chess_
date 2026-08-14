#pragma once

#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "kfc/server/room_directory.hpp"

namespace kfc::server::testing {

/// An in-memory IRoomDirectory for RoomManagerTest/ClientSessionTest -- no
/// Redis server needed to exercise the redirect logic those tests cover.
/// RedisRoomDirectory itself is verified separately, against a real Redis,
/// by the two-worker docker compose acceptance test (see DOCKER/README.md);
/// see test_websocket_end_to_end.cpp for the same "real socket, verified
/// manually, not in the fast suite" choice made for a different dependency.
class FakeRoomDirectory : public IRoomDirectory {
public:
    void register_room(const std::string& room_name, const std::string& worker_url) override {
        std::lock_guard<std::mutex> guard(mutex_);
        owners_[room_name] = worker_url;
    }

    [[nodiscard]] std::optional<std::string> owner_of(const std::string& room_name) override {
        std::lock_guard<std::mutex> guard(mutex_);
        auto it = owners_.find(room_name);
        return it == owners_.end() ? std::nullopt : std::optional<std::string>{it->second};
    }

    void forget_room(const std::string& room_name) override {
        std::lock_guard<std::mutex> guard(mutex_);
        owners_.erase(room_name);
    }

private:
    std::mutex mutex_;
    std::map<std::string, std::string> owners_;
};

}  // namespace kfc::server::testing
