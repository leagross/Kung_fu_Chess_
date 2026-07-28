#pragma once

#include "../../kfc/realtime/cooldown_policy.hpp"

namespace kfc::model {

/// Cooldown after an ordinary move -- a piece must not be movable again
/// while it is still visually resting (long_rest). Longer than
/// JumpCooldownPolicy's, since an ordinary move's rest is the "long rest"
/// while jump-in-place only ever plays a "short rest". The value is tied to
/// the long_rest clip's own frame-count/frames-per-sec in the active asset
/// pack's config.json -- if that art is retimed, this needs updating too.
class StandardCooldownPolicy : public ICooldownPolicy {
public:
    int cooldown_ms() const override;
};

/// The default StandardCooldownPolicy instance -- a named object (not a
/// temporary) so kfc::texttests::Game can bind a reference-typed
/// constructor parameter to it, the same pattern
/// motion_factory.hpp's kDefaultPieceSpeedProvider already uses. A GUI app
/// that wants config-driven cooldowns instead passes its own
/// ICooldownPolicy implementation in Game's constructor.
extern const StandardCooldownPolicy kDefaultStandardCooldownPolicy;

}  // namespace kfc::model
