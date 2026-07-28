#pragma once

#include <cstdint>
#include <functional>
#include <string>

namespace kfc::server {

/// How the server reaches one connected client -- IXWebSocket details stay
/// entirely in WebSocketGameServer; everything below it only ever needs "send
/// this text to that person", so it can all be unit-tested without a real
/// socket.
using SendFn = std::function<void(const std::string&)>;

/// How the server lets one connected client go, once their match is decided.
/// Same reasoning as SendFn: the socket type stays out, so a test can pass a
/// recording lambda (or nothing at all).
using CloseFn = std::function<void()>;

/// Identifies one watcher of one match, for as long as it is watching, so that
/// its connection can be found and dropped again when it closes.
///
/// A player is identified by their colour; a watcher has none, and there can be
/// any number of them, so without a handle there is no way to say *which* one
/// left. 0 is "no watcher" -- the value a seated player's handle carries.
/// Handed out in order and never reused within a match, so a late close from a
/// connection that already left cannot unregister whoever came after it.
using WatcherId = std::uint64_t;

}  // namespace kfc::server
