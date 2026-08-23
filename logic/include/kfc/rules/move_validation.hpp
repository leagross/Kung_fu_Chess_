#pragma once

#include <string>

namespace kfc::model {

/// reason is "ok" for a legal move, otherwise a stable code (see move_reasons.h).
struct [[nodiscard]] MoveValidation {
    bool is_valid;
    std::string reason;
};

}  // namespace kfc::model
