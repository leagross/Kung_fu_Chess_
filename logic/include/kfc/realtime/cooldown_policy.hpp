#pragma once

namespace kfc::model {

/// Strategy interface for how long a piece rests after a motion arrives.
/// Two real implementations exist (Standard, Jump) because the rule is
/// genuinely different between them, not just a parameter -- that is what
/// justifies the interface rather than a single if/else.
class ICooldownPolicy {
public:
    virtual ~ICooldownPolicy() = default;

    /// Milliseconds of rest before the piece may be commanded again.
    [[nodiscard]] virtual int cooldown_ms() const = 0;
};

}  // namespace kfc::model
