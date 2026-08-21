#pragma once

#include <string>
#include <unordered_map>

#include "kfc/graphics/animation/animation_config_loader.hpp"
#include "kfc/graphics/animation/piece_animation_set.hpp"
#include "kfc/graphics/assets/piece_code_scheme.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Loads and caches one PieceAnimationSet per piece folder, so pieces of the
/// same kind+color share a single loaded set.
class PieceAssetLibrary {
public:
    /// scheme must outlive this PieceAssetLibrary.
    explicit PieceAssetLibrary(const IPieceCodeScheme& scheme);

    /// Loads from disk on first request. Throws whatever
    /// AnimationConfigLoader::load or scheme.folder_name throw; callers
    /// decide whether to skip that piece rather than abort.
    const PieceAnimationSet& animation_set_for(kfc::model::PieceKind kind, kfc::model::PieceColor color) const;

private:
    const IPieceCodeScheme& scheme_;
    AnimationConfigLoader loader_;
    mutable std::unordered_map<std::string, PieceAnimationSet> cache_;
};

}  // namespace kfc::graphics
