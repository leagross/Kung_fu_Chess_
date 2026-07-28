#pragma once

namespace kfc::model {

/// Distinguishes an ordinary move from a jump-in-place. Both are tracked as
/// Motion objects by RealTimeArbiter -- the kind only affects how
/// MotionFactory computes duration and cooldown, and nothing else needs to
/// branch on it.
enum class MotionKind {
    Move,
    JumpInPlace,
};

}  // namespace kfc::model
