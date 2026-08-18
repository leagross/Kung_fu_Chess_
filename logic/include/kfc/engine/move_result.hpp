#pragma once

#include <string>

namespace kfc::model {

/// Outcome of a move or jump request. reason is "ok" when accepted, else a
/// stable rejection code (app-level, or copied from RuleEngine).
/// [[nodiscard]] on the type so no caller can silently drop a rejection.
struct [[nodiscard]] MoveResult {
    bool is_accepted;
    std::string reason;
};

}  // namespace kfc::model
