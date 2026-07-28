#include "../../../include/kfc/rules/queen_rule.hpp"

#include "../../../include/kfc/rules/movement_geometry.hpp"

namespace kfc::model {

namespace {
const std::vector<std::pair<int, int>> kQueenDirections = {{-1, 0}, {1, 0}, {0, -1}, {0, 1},
                                                             {-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
}  // namespace

std::vector<Position> QueenRule::legal_destinations(const Board& board, const Piece& piece) const {
    return sliding_destinations(board, piece, kQueenDirections);
}

}  // namespace kfc::model
