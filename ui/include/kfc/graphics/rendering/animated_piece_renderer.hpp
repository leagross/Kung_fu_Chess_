#pragma once

#include <string>
#include <unordered_map>

#include "kfc/graphics/animation/piece_animator_registry.hpp"
#include "kfc/graphics/primitives/img.hpp"

namespace kfc::graphics {

/// Draws every animator PieceAnimatorRegistry currently holds, at its
/// current animated frame and position. Its cache is keyed by full sprite
/// path (not by piece folder, unlike PieceRenderer's) because which file an
/// animated piece needs changes every few ticks as its frame cycles --
/// still bounded, since every state has only a handful of distinct frames
/// that keep repeating.
class AnimatedPieceRenderer {
public:
    /// show_rest_ring: whether to draw the draining "hourglass" overlay over
    /// a resting piece's cell. Defaults on; the app can turn it off (e.g.
    /// while tuning movement speed and wanting one less thing on screen to
    /// reason about) without touching PieceAnimator/Img, which still
    /// compute/support it either way.
    explicit AnimatedPieceRenderer(bool show_rest_ring = true);

    void draw(const PieceAnimatorRegistry& registry, Img& board_image);

private:
    std::unordered_map<std::string, Img> sprite_cache_;
    bool show_rest_ring_;
};

}  // namespace kfc::graphics
