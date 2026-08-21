#pragma once

#include "kfc/model/position.hpp"

namespace kfc::graphics {

/// A location in image pixel space; origin at the image's top-left corner.
struct PixelPoint {
    int x;
    int y;
};

/// Pixel coordinates of cell's top-left corner; exact inverse of
/// kfc::input::BoardMapper::pixel_to_cell (no margin).
PixelPoint cell_top_left(const kfc::model::Position& cell);

}  // namespace kfc::graphics
