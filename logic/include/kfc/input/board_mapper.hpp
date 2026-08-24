#pragma once

#include <optional>

#include "../../kfc/model/position.hpp"

namespace kfc::input {

inline constexpr int kCellSizePixels = 100;

/// Converts pixel coordinates into board cells; knows nothing about pieces, selection, or rules.
class BoardMapper {
public:
    BoardMapper(int board_width, int board_height);

    /// nullopt if the pixel falls outside the board.
    std::optional<kfc::model::Position> pixel_to_cell(int x, int y) const;

private:
    int board_width_;
    int board_height_;
};

}  // namespace kfc::input
