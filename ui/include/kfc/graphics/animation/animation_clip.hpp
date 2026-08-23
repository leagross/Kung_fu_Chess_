#pragma once

#include <filesystem>

#include "kfc/graphics/animation/piece_state_name.hpp"

namespace kfc::graphics {

/// Holds sprites_dir rather than loaded frames: frames are loaded lazily on draw.
struct AnimationClip {
    std::filesystem::path sprites_dir;  // numbered sprite frames (1.png, 2.png, ...)

    int frame_count;

    int frames_per_sec;

    /// If false, holds on the last frame until next_state_when_finished fires.
    bool is_loop;

    /// Meaningful only when is_loop is false.
    PieceStateName next_state_when_finished;
};

}  // namespace kfc::graphics
