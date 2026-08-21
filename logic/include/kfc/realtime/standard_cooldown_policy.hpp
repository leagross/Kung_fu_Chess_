#pragma once

#include "../../kfc/realtime/cooldown_policy.hpp"

namespace kfc::model {

/// Cooldown after an ordinary move (long_rest) -- longer than
/// JumpCooldownPolicy's short_rest. Value tracks the long_rest clip's
/// duration in the active asset pack's config.json.
class StandardCooldownPolicy : public ICooldownPolicy {
public:
    int cooldown_ms() const override;
};

/// Named object so callers can bind a reference-typed constructor param to
/// it; a GUI app wanting config-driven cooldowns passes its own instead.
extern const StandardCooldownPolicy kDefaultStandardCooldownPolicy;

}  // namespace kfc::model
