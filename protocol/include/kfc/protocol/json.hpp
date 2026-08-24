#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "kfc/model/board.hpp"
#include "kfc/protocol/messages.hpp"

namespace kfc::protocol {

/// Largest wire message either side will parse, checked before parsing to
/// avoid allocating/walking an oversized peer frame.
inline constexpr std::size_t kMaxMessageBytes = 1024 * 1024;

/// Builds a BoardSnapshot by reading every occupied cell of board.
[[nodiscard]] BoardSnapshot snapshot_of(const kfc::model::Board& board);

/// Encodes one message as a single JSON text line: {"type": "<name>", "payload": {...}}.
[[nodiscard]] std::string encode(const ClientMessage& message);
[[nodiscard]] std::string encode(const ServerMessage& message);

/// Parses one JSON text line back into a message; nullopt on anything a
/// caller should log and drop rather than crash on (oversized, malformed,
/// unrecognized type, missing required field).
[[nodiscard]] std::optional<ClientMessage> decode_client_message(const std::string& text);
[[nodiscard]] std::optional<ServerMessage> decode_server_message(const std::string& text);

/// The wire text with every password value replaced by "***"; redacts
/// textually so it still works on a message this build can't parse.
[[nodiscard]] std::string redact_for_log(const std::string& text);

}  // namespace kfc::protocol
