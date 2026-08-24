#pragma once

#include <string>
#include <vector>

#include "../../kfc/model/board.hpp"

namespace kfc::io {

/// Builds a Board from grid text: one row per line, space-separated tokens, "." for empty.
class BoardParser {
public:
    /// Throws ParseError: "EMPTY_BOARD", "ROW_WIDTH_MISMATCH", or "UNKNOWN_TOKEN".
    kfc::model::Board parse(const std::vector<std::string>& board_lines) const;
};

}  // namespace kfc::io
