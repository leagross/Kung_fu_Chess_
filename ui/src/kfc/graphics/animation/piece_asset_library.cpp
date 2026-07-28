#include "../../../../include/kfc/graphics/animation/piece_asset_library.hpp"

#include "kfc/graphics/constants.hpp"

namespace kfc::graphics {

PieceAssetLibrary::PieceAssetLibrary(const IPieceCodeScheme& scheme) : scheme_(scheme) {}

const PieceAnimationSet& PieceAssetLibrary::animation_set_for(kfc::model::PieceKind kind,
                                                                kfc::model::PieceColor color) const {
    std::string folder = scheme_.folder_name(kind, color);

    auto cached = cache_.find(folder);
    if (cached == cache_.end()) {
        std::filesystem::path piece_folder = assets_root() / kDefaultAssetPackName / folder;
        cached = cache_.emplace(folder, loader_.load(piece_folder)).first;
    }
    return cached->second;
}

}  // namespace kfc::graphics
