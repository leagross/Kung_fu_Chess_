#pragma once

#include <optional>

#include "../../kfc/model/position.hpp"

namespace kfc::input {

/// Pixel size of one board cell in tests and the minimal UI. Fixed by the
/// design document, not configurable per instance.
inline constexpr int kCellSizePixels = 100;

/// Coordinate Adapter: converts pixel coordinates into board cells. Knows
/// nothing about pieces, selection, or game rules -- only the board's
/// dimensions and the fixed cell size. Owned by the input layer, never the
/// model, so the model stays free of pixels.
class BoardMapper {
public:
    /// board_width/board_height are in cells, matching kfc::model::Board.
    BoardMapper(int board_width, int board_height);

    /// Returns the cell containing pixel (x, y), or std::nullopt if the
    /// pixel falls outside the board.
    std::optional<kfc::model::Position> pixel_to_cell(int x, int y) const;

private:
    int board_width_;
    int board_height_;
};

}  // namespace kfc::input
