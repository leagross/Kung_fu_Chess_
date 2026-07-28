#include "../../../include/kfc/rules/drone_rule.hpp"

#include "../../../include/kfc/rules/movement_geometry.hpp"

namespace kfc::model {

namespace {
const std::vector<std::pair<int, int>> kDroneOffsets = {{-2, 0}, {-1, 0}, {1, 0}, {2, 0},
                                                          {0, -2}, {0, -1}, {0, 1}, {0, 2}};
}  // namespace

std::vector<Position> DroneRule::legal_destinations(const Board& board, const Piece& piece) const {
    return stepping_destinations(board, piece, kDroneOffsets);
}

}  // namespace kfc::model
