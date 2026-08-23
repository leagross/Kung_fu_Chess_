#include "../../../include/kfc/realtime/standard_cooldown_policy.hpp"

namespace kfc::model {

namespace {
// Must be >= the long_rest clip's natural duration, or the piece looks movable while still resting.
constexpr int kStandardCooldownMs = 2500;
}  // namespace

int StandardCooldownPolicy::cooldown_ms() const {
    return kStandardCooldownMs;
}

const StandardCooldownPolicy kDefaultStandardCooldownPolicy;

}  // namespace kfc::model
