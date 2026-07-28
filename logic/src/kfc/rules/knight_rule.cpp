#include "../../../include/kfc/rules/knight_rule.hpp"

#include "../../../include/kfc/rules/movement_geometry.hpp"

namespace kfc::model {

namespace {
const std::vector<std::pair<int, int>> kKnightOffsets = {{-2, -1}, {-2, 1}, {-1, -2}, {-1, 2},
                                                            {1, -2}, {1, 2}, {2, -1}, {2, 1}};
}  // namespace

std::vector<Position> KnightRule::legal_destinations(const Board& board, const Piece& piece) const {
    return stepping_destinations(board, piece, kKnightOffsets);
}

}  // namespace kfc::model
