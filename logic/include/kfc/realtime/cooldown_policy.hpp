#pragma once

namespace kfc::model {

/// Strategy interface for how long a piece rests after a motion arrives.
/// Standard and Jump have genuinely different rules, not just a parameter.
class ICooldownPolicy {
public:
    virtual ~ICooldownPolicy() = default;

    /// Milliseconds of rest before the piece may be commanded again.
    [[nodiscard]] virtual int cooldown_ms() const = 0;
};

}  // namespace kfc::model
