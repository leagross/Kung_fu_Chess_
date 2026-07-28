#include "../../../../include/kfc/graphics/assets/piece_sprite_path.hpp"

#include "kfc/graphics/constants.hpp"

namespace kfc::graphics {

std::filesystem::path idle_sprite_path(const IPieceCodeScheme& scheme, kfc::model::PieceKind kind,
                                        kfc::model::PieceColor color) {
    return assets_root() / kDefaultAssetPackName / scheme.folder_name(kind, color) / kStatesFolderName /
           kIdleStateName / kSpritesFolderName / sprite_frame_filename(kDefaultSpriteFrameIndex);
}

}  // namespace kfc::graphics
