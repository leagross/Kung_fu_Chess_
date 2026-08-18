#include "../../../../include/kfc/graphics/assets/piece_code_scheme.hpp"

#include <unordered_map>

namespace kfc::graphics {

namespace {

// No entry for PieceKind::Drone (no art in pieces_mine); folder_name()
// throwing via .at() lets callers detect and skip an unsupported kind.
const std::unordered_map<kfc::model::PieceKind, char> kKindLetters = {
    {kfc::model::PieceKind::King, 'K'},  {kfc::model::PieceKind::Queen, 'Q'},
    {kfc::model::PieceKind::Rook, 'R'},  {kfc::model::PieceKind::Bishop, 'B'},
    {kfc::model::PieceKind::Knight, 'N'}, {kfc::model::PieceKind::Pawn, 'P'},
};

}  // namespace

std::string PiecesMineCodeScheme::folder_name(kfc::model::PieceKind kind, kfc::model::PieceColor color) const {
    char color_letter = (color == kfc::model::PieceColor::White) ? 'w' : 'b';
    return std::string(1, color_letter) + kKindLetters.at(kind);
}

}  // namespace kfc::graphics
