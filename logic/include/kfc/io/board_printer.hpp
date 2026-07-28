#pragma once

#include <string>

#include "../../kfc/model/board.hpp"

namespace kfc::io {

/// The io/ layer counterpart to BoardParser: turns a kfc::model::Board back
/// into text. Read-only with respect to Board.
class BoardPrinter {
public:
    /// One row per line, tokens separated by a single space, "." for an
    /// empty cell. The exact inverse of BoardParser::parse.
    std::string print(const kfc::model::Board& board) const;
};

}  // namespace kfc::io
