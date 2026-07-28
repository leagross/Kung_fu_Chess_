#pragma once

#include "../../kfc/realtime/cooldown_policy.hpp"

namespace kfc::model {

/// Cooldown after a jump-in-place -- a piece must not be movable again
/// while it is still visually resting (short_rest). Its cooldown is what
/// makes the race-condition timing in test_jump_in_place.cpp meaningful.
/// The value is tied to the short_rest clip's own frame-count/frames-per-sec
/// in the active asset pack's config.json -- if that art is retimed, this
/// needs updating too.
class JumpCooldownPolicy : public ICooldownPolicy {
public:
    int cooldown_ms() const override;
};

/// The default JumpCooldownPolicy instance -- see
/// kDefaultStandardCooldownPolicy for why this needs to be a named object.
extern const JumpCooldownPolicy kDefaultJumpCooldownPolicy;

}  // namespace kfc::model
