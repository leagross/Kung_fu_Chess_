#include "../../../include/kfc/rules/king_rule.hpp"

#include "../../../include/kfc/rules/movement_geometry.hpp"

namespace kfc::model {

namespace {
const std::vector<std::pair<int, int>> kKingOffsets = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1},
                                                          {0, 1}, {1, -1}, {1, 0}, {1, 1}};
}  // namespace

std::vector<Position> KingRule::legal_destinations(const Board& board, const Piece& piece) const {
    return stepping_destinations(board, piece, kKingOffsets);
}

}  // namespace kfc::model
