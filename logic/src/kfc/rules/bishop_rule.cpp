#include "../../../include/kfc/rules/bishop_rule.hpp"

#include "../../../include/kfc/rules/movement_geometry.hpp"

namespace kfc::model {

namespace {
const std::vector<std::pair<int, int>> kBishopDirections = {{-1, -1}, {-1, 1}, {1, -1}, {1, 1}};
}  // namespace

std::vector<Position> BishopRule::legal_destinations(const Board& board, const Piece& piece) const {
    return sliding_destinations(board, piece, kBishopDirections);
}

}  // namespace kfc::model
