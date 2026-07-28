#pragma once

#include "kfc/model/position.hpp"

namespace kfc::graphics {

/// A location in image pixel space -- x is horizontal, y is vertical, origin
/// at the image's top-left corner. Its own named type (not a raw
/// std::pair<int,int>) for the same reason kfc::model::Position isn't a
/// std::pair<int,int>: .x/.y reads at the call site instead of
/// .first/.second.
struct PixelPoint {
    int x;
    int y;
};

/// Pixel coordinates of cell's top-left corner, using the same
/// kfc::input::kCellSizePixels BoardMapper uses -- the exact inverse of
/// kfc::input::BoardMapper::pixel_to_cell, with no margin, matching
/// BoardMapper's own no-margin assumption. A piece sprite drawn at this
/// point must be no larger than one cell, or it spills into neighbors.
PixelPoint cell_top_left(const kfc::model::Position& cell);

}  // namespace kfc::graphics
