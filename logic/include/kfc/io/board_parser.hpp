#pragma once

#include <string>
#include <vector>

#include "../../kfc/model/board.hpp"

namespace kfc::io {

/// Builds a kfc::model::Board from board-grid text: one row per line, tokens
/// space-separated, "." for an empty cell, piece ids assigned in reading
/// order. This is the io/ layer component the design document names --
/// shared by the real app and by text integration tests, never duplicated
/// inside a test-only namespace.
class BoardParser {
public:
    /// Rejects a grid with no rows ("EMPTY_BOARD"), rows of inconsistent
    /// width ("ROW_WIDTH_MISMATCH"), and any token that isn't "." or a
    /// recognized piece token ("UNKNOWN_TOKEN"). Throws ParseError.
    kfc::model::Board parse(const std::vector<std::string>& board_lines) const;
};

}  // namespace kfc::io
