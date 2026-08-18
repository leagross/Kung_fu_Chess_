#pragma once

#include <filesystem>

#include "kfc/graphics/assets/piece_code_scheme.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Path to a piece's idle-state first sprite frame, e.g.
/// .../pieces_mine/wK/states/idle/sprites/1.png. Delegates folder naming to
/// scheme; only knows the shared states/<state>/sprites/<frame> layout.
std::filesystem::path idle_sprite_path(const IPieceCodeScheme& scheme, kfc::model::PieceKind kind,
                                        kfc::model::PieceColor color);

}  // namespace kfc::graphics
