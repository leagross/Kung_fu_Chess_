#pragma once

#include <optional>
#include <string>

#include "../../kfc/model/piece.hpp"

namespace kfc::io {

/// Two-character notation: color prefix ('w'/'b') + kind letter (K Q R B N P D).
struct PieceToken {
    kfc::model::PieceColor color;
    kfc::model::PieceKind kind;
};

/// nullopt for anything not a recognized color+kind pair (including "." for empty cell).
std::optional<PieceToken> parse_piece_token(const std::string& token);

/// Inverse of parse_piece_token.
std::string piece_token_text(kfc::model::PieceColor color, kfc::model::PieceKind kind);

char letter_for_kind(kfc::model::PieceKind kind);

}  // namespace kfc::io
