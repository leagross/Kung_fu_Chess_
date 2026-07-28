#pragma once

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

}  // namespace kfc::server
