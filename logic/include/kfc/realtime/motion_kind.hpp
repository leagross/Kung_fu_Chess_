#pragma once

namespace kfc::model {

/// Only affects how MotionFactory computes duration and cooldown.
enum class MotionKind {
    Move,
    JumpInPlace,
};

}  // namespace kfc::model
