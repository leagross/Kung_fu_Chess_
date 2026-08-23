#pragma once

#include "../../kfc/realtime/cooldown_policy.hpp"

namespace kfc::model {

/// Cooldown after a jump-in-place.
class JumpCooldownPolicy : public ICooldownPolicy {
public:
    int cooldown_ms() const override;
};

extern const JumpCooldownPolicy kDefaultJumpCooldownPolicy;

}  // namespace kfc::model
