#include "../../../include/kfc/io/piece_token.hpp"

using kfc::model::PieceColor;
using kfc::model::PieceKind;

namespace kfc::io {

namespace {

std::optional<PieceKind> kind_for_letter(char letter) {
    switch (letter) {
        case 'K': return PieceKind::King;
        case 'Q': return PieceKind::Queen;
        case 'R': return PieceKind::Rook;
        case 'B': return PieceKind::Bishop;
        case 'N': return PieceKind::Knight;
        case 'P': return PieceKind::Pawn;
        case 'D': return PieceKind::Drone;
        default: return std::nullopt;
    }
}

}  // namespace

char letter_for_kind(PieceKind kind) {
    switch (kind) {
        case PieceKind::King: return 'K';
        case PieceKind::Queen: return 'Q';
        case PieceKind::Rook: return 'R';
        case PieceKind::Bishop: return 'B';
        case PieceKind::Knight: return 'N';
        case PieceKind::Pawn: return 'P';
        case PieceKind::Drone: return 'D';
    }
    return '?';
}

std::optional<PieceToken> parse_piece_token(const std::string& token) {
    if (token.size() != 2 || (token[0] != 'w' && token[0] != 'b')) {
        return std::nullopt;
    }
    std::optional<PieceKind> kind = kind_for_letter(token[1]);
    if (!kind.has_value()) {
        return std::nullopt;
    }
    PieceColor color = (token[0] == 'w') ? PieceColor::White : PieceColor::Black;
    return PieceToken{color, *kind};
}

std::string piece_token_text(PieceColor color, PieceKind kind) {
    std::string text;
    text += (color == PieceColor::White) ? 'w' : 'b';
    text += letter_for_kind(kind);
    return text;
}

}  // namespace kfc::io
