#pragma once

#include <filesystem>

#include "kfc/graphics/animation/piece_animation_set.hpp"

namespace kfc::graphics {

/// The only class in the animation layer that touches the filesystem or parses JSON.
class AnimationConfigLoader {
public:
    /// piece_folder is the piece's own folder (e.g. <pack root>/wK), not the states/ subfolder.
    PieceAnimationSet load(const std::filesystem::path& piece_folder) const;
};

}  // namespace kfc::graphics
