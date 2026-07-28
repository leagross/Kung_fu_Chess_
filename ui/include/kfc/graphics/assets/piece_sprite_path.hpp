#pragma once

#include <filesystem>

#include "kfc/graphics/assets/piece_code_scheme.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Path to a piece's idle-state first sprite frame within the default asset
/// pack, e.g. .../pieces_mine/wK/states/idle/sprites/1.png. Delegates the
/// piece-folder-naming step to scheme, so this function stays correct
/// regardless of which asset pack's naming convention is active -- it only
/// knows the shared states/<state>/sprites/<frame> layout every pack agrees
/// on, never a specific pack's folder-naming rule itself.
std::filesystem::path idle_sprite_path(const IPieceCodeScheme& scheme, kfc::model::PieceKind kind,
                                        kfc::model::PieceColor color);

}  // namespace kfc::graphics
