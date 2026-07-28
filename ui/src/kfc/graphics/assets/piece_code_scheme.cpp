#include "../../../../include/kfc/graphics/assets/piece_code_scheme.hpp"

#include <unordered_map>

namespace kfc::graphics {

namespace {

// One letter per PieceKind pieces_mine ships art for. Deliberately has no
// entry for PieceKind::Drone -- there is no drone folder in pieces_mine, and
// folder_name() throwing for it (via .at()) is what lets the future
// PieceRenderer detect "no asset for this kind" and skip drawing, instead of
// silently returning a folder name that doesn't exist on disk.
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
