#pragma once

#include <unordered_map>

#include "kfc/graphics/animation/animation_clip.hpp"
#include "kfc/graphics/animation/piece_state_name.hpp"

namespace kfc::graphics {

/// One piece's five AnimationClips, read-only after construction.
class PieceAnimationSet {
public:
    /// clips must have an entry for every PieceStateName value.
    explicit PieceAnimationSet(std::unordered_map<PieceStateName, AnimationClip> clips);

    /// Throws std::out_of_range if state has no entry (a loader bug, not a
    /// normal runtime condition).
    const AnimationClip& clip(PieceStateName state) const;

private:
    std::unordered_map<PieceStateName, AnimationClip> clips_;
};

}  // namespace kfc::graphics
