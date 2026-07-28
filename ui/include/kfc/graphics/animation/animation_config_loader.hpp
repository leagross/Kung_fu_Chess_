#pragma once

#include <filesystem>

#include "kfc/graphics/animation/piece_animation_set.hpp"

namespace kfc::graphics {

/// Reads one piece's entire states/ folder (all five kAllPieceStateNames
/// subfolders: config.json + sprites/) into a PieceAnimationSet. The only
/// class in the animation layer that touches a filesystem path or parses
/// JSON -- everything downstream (PieceAnimationSet, AnimationClip) is pure
/// data with no idea where it came from.
class AnimationConfigLoader {
public:
    /// piece_folder is one piece's own folder, e.g.
    /// <pack root>/wK -- not the states/ subfolder itself. Throws
    /// std::runtime_error if any state's config.json is missing or
    /// malformed, or if a config.json's next_state_when_finished names a
    /// state parse_piece_state_name doesn't recognize.
    PieceAnimationSet load(const std::filesystem::path& piece_folder) const;
};

}  // namespace kfc::graphics
