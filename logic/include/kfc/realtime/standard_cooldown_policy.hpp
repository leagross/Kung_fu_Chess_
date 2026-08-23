#pragma once

#include "../../kfc/realtime/cooldown_policy.hpp"

namespace kfc::model {

/// Cooldown after an ordinary move; longer than JumpCooldownPolicy's.
class StandardCooldownPolicy : public ICooldownPolicy {
public:
    int cooldown_ms() const override;
};

extern const StandardCooldownPolicy kDefaultStandardCooldownPolicy;

}  // namespace kfc::model
