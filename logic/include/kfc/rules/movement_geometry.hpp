#pragma once

#include <utility>
#include <vector>

#include "../../kfc/model/board.hpp"
#include "../../kfc/model/piece.hpp"
#include "../../kfc/model/position.hpp"

namespace kfc::model {

/// Shared by Rook/Bishop/Queen: walks each direction until the board edge, a friendly piece, or a captured enemy.
[[nodiscard]] std::vector<Position> sliding_destinations(const Board& board, const Piece& piece,
                                                         const std::vector<std::pair<int, int>>& directions);

/// Shared by Knight/King/Drone: checks each offset once, ignoring what lies between.
[[nodiscard]] std::vector<Position> stepping_destinations(const Board& board, const Piece& piece,
                                                          const std::vector<std::pair<int, int>>& offsets);

}  // namespace kfc::model
