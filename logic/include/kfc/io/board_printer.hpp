#pragma once

#include <string>

#include "../../kfc/model/board.hpp"

namespace kfc::io {

/// Inverse of BoardParser::parse: turns a Board back into grid text.
class BoardPrinter {
public:
    std::string print(const kfc::model::Board& board) const;
};

}  // namespace kfc::io
