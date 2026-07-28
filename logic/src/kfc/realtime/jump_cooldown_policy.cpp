#include "../../../include/kfc/realtime/jump_cooldown_policy.hpp"

namespace kfc::model {

namespace {
// Must be >= the short_rest clip's own natural duration (currently 5 frames
// at 6 frames/sec = ~833ms across every piece in the active asset pack) --
// otherwise the piece becomes movable again while it is still visually
// resting. See PieceAnimator::advance's natural_duration_ms computation.
constexpr int kJumpCooldownMs = 834;
}  // namespace

int JumpCooldownPolicy::cooldown_ms() const {
    return kJumpCooldownMs;
}

const JumpCooldownPolicy kDefaultJumpCooldownPolicy;

}  // namespace kfc::model
