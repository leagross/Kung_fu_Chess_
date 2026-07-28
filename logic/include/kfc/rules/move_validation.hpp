#pragma once

#include <string>

namespace kfc::model {

/// Result of asking RuleEngine whether a move is currently legal at the rule
/// level. reason is always present: "ok" for a legal move, otherwise a
/// stable, machine-readable code such as "outside_board" (see move_reasons.h).
/// [[nodiscard]] on the type: RuleEngine::validate_move exists only to be
/// asked "is this legal, and if not why" -- discarding the answer is always
/// a mistake.
struct [[nodiscard]] MoveValidation {
    bool is_valid;
    std::string reason;
};

}  // namespace kfc::model
