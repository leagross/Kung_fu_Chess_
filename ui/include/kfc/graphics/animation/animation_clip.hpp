#pragma once

#include <filesystem>

#include "kfc/graphics/animation/piece_state_name.hpp"

namespace kfc::graphics {

/// Holds sprites_dir rather than loaded frames: frames are loaded lazily
/// on draw, not eagerly here.
struct AnimationClip {
    /// Directory holding numbered sprite frames (1.png, 2.png, ...).
    std::filesystem::path sprites_dir;

    int frame_count;

    /// Frames to show per second.
    int frames_per_sec;

    /// If false, holds on the last frame until next_state_when_finished fires.
    bool is_loop;

    /// State to enter on completion; meaningful only when is_loop is false.
    PieceStateName next_state_when_finished;
};

}  // namespace kfc::graphics
