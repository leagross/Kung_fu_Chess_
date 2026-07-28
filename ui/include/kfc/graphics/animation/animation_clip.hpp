#pragma once

#include <filesystem>

#include "kfc/graphics/animation/piece_state_name.hpp"

namespace kfc::graphics {

/// One state's animation data, parsed straight from its config.json plus
/// the frame count counted from its sprites/ folder -- everything a future
/// PieceAnimator needs to know about this state, and nothing about how to
/// draw it. Deliberately holds sprites_dir (a location), not loaded Img
/// frames: which frame is needed changes every tick, so loading happens
/// lazily wherever a frame is actually drawn (the same lazy-cache pattern
/// PieceRenderer already uses for idle sprites), not eagerly here.
struct AnimationClip {
    /// Directory holding this state's numbered sprite frames
    /// (1.png, 2.png, ...) -- join with sprite_frame_filename(n) to name one.
    std::filesystem::path sprites_dir;

    /// How many numbered frames exist in sprites_dir.
    int frame_count;

    /// How many of those frames to show per second.
    int frames_per_sec;

    /// True if the animation restarts from frame 1 after its last frame;
    /// false if it holds on the last frame until a state transition happens
    /// (driven by next_state_when_finished, once PieceAnimator exists).
    bool is_loop;

    /// Which state to enter once this one's animation completes -- only
    /// meaningful when is_loop is false; a looping state never "finishes".
    PieceStateName next_state_when_finished;
};

}  // namespace kfc::graphics
