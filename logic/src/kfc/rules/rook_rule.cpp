#include "../../../include/kfc/rules/rook_rule.hpp"

#include "../../../include/kfc/rules/movement_geometry.hpp"

namespace kfc::model {

namespace {
const std::vector<std::pair<int, int>> kRookDirections = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
}  // namespace

std::vector<Position> RookRule::legal_destinations(const Board& board, const Piece& piece) const {
    return sliding_destinations(board, piece, kRookDirections);
}

}  // namespace kfc::model
