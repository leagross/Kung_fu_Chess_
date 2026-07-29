#pragma once

#include <cstddef>
#include <optional>
#include <string>

#include "kfc/model/board.hpp"
#include "kfc/protocol/messages.hpp"

namespace kfc::protocol {

/// The largest wire message either side will parse, in bytes.
///
/// Sized for the biggest legitimate message by a wide margin: a Welcome for a
/// full board plus the whole move history of a long game runs to a few tens of
/// kilobytes. Anything past this is not a game -- and parsing it would mean
/// allocating and walking whatever a peer chose to send, which is the cheapest
/// possible way to exhaust a server's memory. Checked before parsing, since
/// after parsing is too late.
inline constexpr std::size_t kMaxMessageBytes = 1024 * 1024;

/// Builds a BoardSnapshot by reading every occupied cell of board -- the
/// only place Board's public piece_at()/width()/height() API is walked for
/// networking purposes; Board itself stays untouched.
[[nodiscard]] BoardSnapshot snapshot_of(const kfc::model::Board& board);

/// Encodes one message as a single JSON text line: {"type": "<name>",
/// "payload": {...}}. The same envelope both sides log verbatim (see the
/// CTD SERVER lecture's logging requirement) and send over the socket.
[[nodiscard]] std::string encode(const ClientMessage& message);
[[nodiscard]] std::string encode(const ServerMessage& message);

/// Parses one JSON text line back into a message. std::nullopt for a text
/// longer than kMaxMessageBytes, malformed JSON, an unrecognized "type", or a
/// payload missing a required field -- callers (Match/ServerLink) log and drop the message rather than
/// crash on a bad frame from a misbehaving or out-of-date peer. [[nodiscard]]:
/// the returned optional *is* the parse outcome -- dropping it means acting
/// on an unvalidated frame.
[[nodiscard]] std::optional<ClientMessage> decode_client_message(const std::string& text);
[[nodiscard]] std::optional<ServerMessage> decode_server_message(const std::string& text);

/// The same wire text with every password value replaced by "***".
///
/// **Every log line that prints a raw protocol message must go through this.**
/// Login carries the password in clear (it is checked server-side and never
/// stored client-side -- see Login), so logging the message verbatim writes
/// real passwords into kfc_server.log and kfc_gui_app.log, in plain text, on
/// disk, forever. Those files are not secrets, are easy to share while
/// debugging, and outlive the session.
///
/// Redacts textually rather than by decoding and re-encoding, so it is also
/// correct for a message this build cannot parse -- a malformed or
/// newer-than-us Login still gets its password stripped rather than logged
/// whole because the decode failed.
[[nodiscard]] std::string redact_for_log(const std::string& text);

}  // namespace kfc::protocol
