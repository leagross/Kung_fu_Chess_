#pragma once

#include <unordered_map>

#include "kfc/graphics/animation/animation_clip.hpp"
#include "kfc/graphics/animation/piece_state_name.hpp"

namespace kfc::graphics {

/// One piece's five AnimationClips (idle/move/jump/short_rest/long_rest),
/// built once by AnimationConfigLoader and read many times afterward (by a
/// future PieceAnimator, once per state transition). Read-only by design --
/// nothing downstream of loading should ever need to mutate a clip.
class PieceAnimationSet {
public:
    /// clips must already have an entry for every kAllPieceStateNames value
    /// -- AnimationConfigLoader::load is the only intended caller, and it
    /// guarantees this by construction.
    explicit PieceAnimationSet(std::unordered_map<PieceStateName, AnimationClip> clips);

    /// The clip for state. Throws std::out_of_range if state has no entry --
    /// which, given the constructor's contract, means a bug in the loader,
    /// not a normal runtime condition callers should expect to handle.
    const AnimationClip& clip(PieceStateName state) const;

private:
    std::unordered_map<PieceStateName, AnimationClip> clips_;
};

}  // namespace kfc::graphics
