#pragma once

#include <string>

namespace kfc::model {

/// reason is "ok" when accepted, else a stable rejection code.
struct [[nodiscard]] MoveResult {
    bool is_accepted;
    std::string reason;
};

}  // namespace kfc::model
