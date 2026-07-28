#pragma once

#include <string>
#include <unordered_map>

#include "kfc/graphics/animation/animation_config_loader.hpp"
#include "kfc/graphics/animation/piece_animation_set.hpp"
#include "kfc/graphics/assets/piece_code_scheme.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Loads (and caches) one PieceAnimationSet per piece folder -- every one
/// of the, say, 8 white pawns on a board shares a single loaded set instead
/// of each re-reading and re-parsing the same 5 config.json files. The
/// single load point for animation data, the same role PieceRenderer's
/// sprite cache played for plain idle sprites before this class existed.
class PieceAssetLibrary {
public:
    /// scheme must outlive this PieceAssetLibrary.
    explicit PieceAssetLibrary(const IPieceCodeScheme& scheme);

    /// The PieceAnimationSet for this kind+color, loading it from disk on
    /// first request. Throws whatever AnimationConfigLoader::load throws
    /// (missing/malformed config) or whatever scheme.folder_name throws
    /// (unknown kind, e.g. Drone) -- callers (PieceAnimatorRegistry) decide
    /// whether to skip that piece rather than abort.
    const PieceAnimationSet& animation_set_for(kfc::model::PieceKind kind, kfc::model::PieceColor color) const;

private:
    const IPieceCodeScheme& scheme_;
    AnimationConfigLoader loader_;
    mutable std::unordered_map<std::string, PieceAnimationSet> cache_;
};

}  // namespace kfc::graphics
