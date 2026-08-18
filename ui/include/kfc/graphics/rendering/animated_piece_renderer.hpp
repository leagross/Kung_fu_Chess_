#pragma once

#include <string>
#include <unordered_map>

#include "kfc/graphics/animation/piece_animator_registry.hpp"
#include "kfc/graphics/primitives/img.hpp"

namespace kfc::graphics {

/// Draws every animator PieceAnimatorRegistry currently holds, at its
/// current animated frame and position. Cache is keyed by full sprite path
/// (not piece folder) since which file is needed changes as the frame cycles.
class AnimatedPieceRenderer {
public:
    /// show_rest_ring toggles the draining "hourglass" overlay over a
    /// resting piece's cell.
    explicit AnimatedPieceRenderer(bool show_rest_ring = true);

    void draw(const PieceAnimatorRegistry& registry, Img& board_image);

private:
    std::unordered_map<std::string, Img> sprite_cache_;
    bool show_rest_ring_;
};

}  // namespace kfc::graphics
