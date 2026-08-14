#include "kfc/server/redis_room_directory.hpp"

#include <stdexcept>
#include <utility>

// Not <hiredis/hiredis.h>: FetchContent builds straight from hiredis's own
// source tree, which has no install step and therefore no hiredis/ subdir --
// the "hiredis/" prefix only exists once headers are actually installed
// system-wide (see hiredis's own CMakeLists.txt, $<INSTALL_INTERFACE:include>
// vs. $<BUILD_INTERFACE:...>).
#include <hiredis.h>

namespace kfc::server {

void RedisRoomDirectory::ContextDeleter::operator()(redisContext* context) const {
    if (context != nullptr) {
        redisFree(context);
    }
}

RedisRoomDirectory::RedisRoomDirectory(const std::string& host, int port)
    : context_(redisConnect(host.c_str(), port)) {
    if (!context_ || context_->err != 0) {
        std::string message = context_ ? context_->errstr : "redisConnect returned null";
        throw std::runtime_error("RedisRoomDirectory: cannot connect to " + host + ":" + std::to_string(port) +
                                 ": " + message);
    }
}

RedisRoomDirectory::~RedisRoomDirectory() = default;

void RedisRoomDirectory::register_room(const std::string& room_name, const std::string& worker_url) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto* reply = static_cast<redisReply*>(redisCommand(context_.get(), "SET room:%s %s EX %d", room_name.c_str(),
                                                         worker_url.c_str(), kEntryTtlSeconds));
    if (reply != nullptr) {
        freeReplyObject(reply);
    }
}

std::optional<std::string> RedisRoomDirectory::owner_of(const std::string& room_name) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto* reply = static_cast<redisReply*>(redisCommand(context_.get(), "GET room:%s", room_name.c_str()));
    if (reply == nullptr) {
        return std::nullopt;  // connection-level failure -- treated as "don't know", not a crash
    }
    std::optional<std::string> result;
    if (reply->type == REDIS_REPLY_STRING) {
        result = std::string(reply->str, reply->len);
    }
    freeReplyObject(reply);
    return result;
}

void RedisRoomDirectory::forget_room(const std::string& room_name) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto* reply = static_cast<redisReply*>(redisCommand(context_.get(), "DEL room:%s", room_name.c_str()));
    if (reply != nullptr) {
        freeReplyObject(reply);
    }
}

}  // namespace kfc::server
