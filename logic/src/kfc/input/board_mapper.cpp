#include "../../../include/kfc/input/board_mapper.hpp"

namespace kfc::input {

BoardMapper::BoardMapper(int board_width, int board_height)
    : board_width_(board_width), board_height_(board_height) {}

std::optional<kfc::model::Position> BoardMapper::pixel_to_cell(int x, int y) const {
    if (x < 0 || y < 0) {
        return std::nullopt;
    }

    int col = x / kCellSizePixels;
    int row = y / kCellSizePixels;

    if (col < 0 || col >= board_width_ || row < 0 || row >= board_height_) {
        return std::nullopt;
    }

    return kfc::model::Position{row, col};
}

}  // namespace kfc::input
