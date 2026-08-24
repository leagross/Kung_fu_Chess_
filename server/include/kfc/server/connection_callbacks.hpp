#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace kfc::server {

/// Keeps IXWebSocket out of everything below WebSocketGameServer, so it can be unit-tested without a real socket.
using SendFn = std::function<void(const std::string&)>;

using CloseFn = std::function<void()>;

/// 0 means "no watcher" (a seated player's value); handed out in order, never reused within a match.
using WatcherId = std::uint64_t;

}  // namespace kfc::server
