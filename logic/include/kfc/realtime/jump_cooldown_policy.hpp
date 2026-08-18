#pragma once

#include "../../kfc/realtime/cooldown_policy.hpp"

namespace kfc::model {

/// Cooldown after a jump-in-place (short_rest). Value tracks the short_rest
/// clip's duration in the active asset pack's config.json.
class JumpCooldownPolicy : public ICooldownPolicy {
public:
    int cooldown_ms() const override;
};

/// Named object so callers can bind a reference-typed constructor param to it.
extern const JumpCooldownPolicy kDefaultJumpCooldownPolicy;

}  // namespace kfc::model
