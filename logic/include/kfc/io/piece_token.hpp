#pragma once

#include <optional>
#include <string>

#include "../../kfc/model/piece.hpp"

namespace kfc::io {

/// Two-character notation for a piece: color prefix ('w'/'b') + kind letter
/// (K Q R B N P D). Shared by BoardParser and BoardPrinter so the mapping
/// exists in exactly one place.
struct PieceToken {
    kfc::model::PieceColor color;
    kfc::model::PieceKind kind;
};

/// Parses a token like "wK" or "bD". Returns std::nullopt for anything that
/// isn't a recognized color+kind pair -- including ".", which callers treat
/// as "empty cell" separately, not as a piece.
std::optional<PieceToken> parse_piece_token(const std::string& token);

/// The exact inverse of parse_piece_token: color+kind back to "wK"/"bD" etc.
std::string piece_token_text(kfc::model::PieceColor color, kfc::model::PieceKind kind);

/// The single letter for a kind (K Q R B N P D), with no color prefix.
char letter_for_kind(kfc::model::PieceKind kind);

}  // namespace kfc::io
