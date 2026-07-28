#pragma once

#include <utility>
#include <vector>

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Shared sliding-movement geometry used by Rook, Bishop, and Queen: from
/// piece's cell, walks each (row_step, col_step) direction one cell at a
/// time, stopping at the board edge, a friendly piece (excluded), or an
/// enemy piece (included, then stops).
[[nodiscard]] std::vector<Position> sliding_destinations(const Board& board, const Piece& piece,
                                                         const std::vector<std::pair<int, int>>& directions);

/// Shared single-step geometry used by Knight and King: checks each
/// (row_offset, col_offset) exactly once, ignoring what lies between piece's
/// cell and the candidate -- there is nothing "between" a knight's jump, and
/// a king only ever considers adjacent cells anyway.
[[nodiscard]] std::vector<Position> stepping_destinations(const Board& board, const Piece& piece,
                                                          const std::vector<std::pair<int, int>>& offsets);

}  // namespace kfc::model
