#include "../../../include/kfc/realtime/standard_cooldown_policy.hpp"

namespace kfc::model {

namespace {
// Must be >= the long_rest clip's own natural duration (currently 5 frames
// at 2 frames/sec = 2500ms across every piece in the active asset pack) --
// otherwise the piece becomes movable again while it is still visually
// resting. See PieceAnimator::advance's natural_duration_ms computation.
constexpr int kStandardCooldownMs = 2500;
}  // namespace

int StandardCooldownPolicy::cooldown_ms() const {
    return kStandardCooldownMs;
}

const StandardCooldownPolicy kDefaultStandardCooldownPolicy;

}  // namespace kfc::model
