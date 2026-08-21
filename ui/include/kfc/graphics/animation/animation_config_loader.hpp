#pragma once

#include <filesystem>

#include "kfc/graphics/animation/piece_animation_set.hpp"

namespace kfc::graphics {

/// Reads one piece's states/ folder into a PieceAnimationSet. The only
/// class in the animation layer that touches the filesystem or parses JSON.
class AnimationConfigLoader {
public:
    /// piece_folder is the piece's own folder (e.g. <pack root>/wK), not
    /// the states/ subfolder. Throws std::runtime_error on missing/malformed
    /// config.json or an unrecognized next_state_when_finished.
    PieceAnimationSet load(const std::filesystem::path& piece_folder) const;
};

}  // namespace kfc::graphics
