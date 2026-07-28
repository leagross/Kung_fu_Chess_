#pragma once

#include <optional>
#include <string>

#include "kfc/model/board.hpp"
#include "kfc/protocol/messages.hpp"

namespace kfc::protocol {

/// Builds a BoardSnapshot by reading every occupied cell of board -- the
/// only place Board's public piece_at()/width()/height() API is walked for
/// networking purposes; Board itself stays untouched.
[[nodiscard]] BoardSnapshot snapshot_of(const kfc::model::Board& board);

/// Encodes one message as a single JSON text line: {"type": "<name>",
/// "payload": {...}}. The same envelope both sides log verbatim (see the
/// CTD SERVER lecture's logging requirement) and send over the socket.
[[nodiscard]] std::string encode(const ClientMessage& message);
[[nodiscard]] std::string encode(const ServerMessage& message);

/// Parses one JSON text line back into a message. std::nullopt for
/// malformed JSON, an unrecognized "type", or a payload missing a required
/// field -- callers (Match/ServerLink) log and drop the message rather than
/// crash on a bad frame from a misbehaving or out-of-date peer. [[nodiscard]]:
/// the returned optional *is* the parse outcome -- dropping it means acting
/// on an unvalidated frame.
[[nodiscard]] std::optional<ClientMessage> decode_client_message(const std::string& text);
[[nodiscard]] std::optional<ServerMessage> decode_server_message(const std::string& text);

}  // namespace kfc::protocol
