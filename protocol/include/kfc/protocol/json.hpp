#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "kfc/model/board.hpp"
#include "kfc/protocol/messages.hpp"

namespace kfc::protocol {

/// Largest wire message either side will parse, in bytes; checked before
/// parsing so an oversized frame can't force allocating/walking peer data.
inline constexpr std::size_t kMaxMessageBytes = 1024 * 1024;

/// Builds a BoardSnapshot by reading every occupied cell of board.
[[nodiscard]] BoardSnapshot snapshot_of(const kfc::model::Board& board);

/// Encodes one message as a single JSON text line: {"type": "<name>",
/// "payload": {...}}.
[[nodiscard]] std::string encode(const ClientMessage& message);
[[nodiscard]] std::string encode(const ServerMessage& message);

/// Parses one JSON text line back into a message. std::nullopt for a text
/// longer than kMaxMessageBytes, malformed JSON, an unrecognized "type", or a
/// payload missing a required field; callers log and drop rather than crash
/// on a bad frame.
[[nodiscard]] std::optional<ClientMessage> decode_client_message(const std::string& text);
[[nodiscard]] std::optional<ServerMessage> decode_server_message(const std::string& text);

/// The same wire text with every password value replaced by "***". Every log
/// line that prints a raw protocol message must go through this, since Login
/// carries the password in clear. Redacts textually (not decode/re-encode)
/// so it still strips the password from a message this build can't parse.
[[nodiscard]] std::string redact_for_log(const std::string& text);

}  // namespace kfc::protocol
