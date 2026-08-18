#pragma once

#include <string>

namespace kfc::model {

/// Result of asking RuleEngine whether a move is legal. reason is always
/// present: "ok" for a legal move, otherwise a stable machine-readable code
/// (see move_reasons.h).
struct [[nodiscard]] MoveValidation {
    bool is_valid;
    std::string reason;
};

}  // namespace kfc::model
