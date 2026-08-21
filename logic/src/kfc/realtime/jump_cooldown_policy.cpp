#include "../../../include/kfc/realtime/jump_cooldown_policy.hpp"

namespace kfc::model {

namespace {
// Must be >= the short_rest clip's natural duration (~833ms at 5 frames /
// 6fps), or the piece would become movable while still visually resting.
constexpr int kJumpCooldownMs = 834;
}  // namespace

int JumpCooldownPolicy::cooldown_ms() const {
    return kJumpCooldownMs;
}

const JumpCooldownPolicy kDefaultJumpCooldownPolicy;

}  // namespace kfc::model
