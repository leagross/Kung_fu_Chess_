#include "../../../../include/kfc/graphics/animation/piece_animation_set.hpp"

#include <utility>

namespace kfc::graphics {

PieceAnimationSet::PieceAnimationSet(std::unordered_map<PieceStateName, AnimationClip> clips)
    : clips_(std::move(clips)) {}

const AnimationClip& PieceAnimationSet::clip(PieceStateName state) const {
    return clips_.at(state);
}

}  // namespace kfc::graphics
