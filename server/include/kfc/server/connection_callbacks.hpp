#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace kfc::server {

/// Sends text to one connected client. Keeps IXWebSocket out of everything
/// below WebSocketGameServer, so it can be unit-tested without a real socket.
using SendFn = std::function<void(const std::string&)>;

/// Closes one connected client's connection. Same reasoning as SendFn.
using CloseFn = std::function<void()>;

/// Identifies one watcher of a match so its connection can be found and
/// dropped when it closes. 0 means "no watcher" (a seated player's value).
/// Handed out in order, never reused within a match.
using WatcherId = std::uint64_t;

}  // namespace kfc::server
