#include "../../../../include/kfc/graphics/geometry/board_geometry.hpp"

#include "kfc/input/board_mapper.hpp"

namespace kfc::graphics {

PixelPoint cell_top_left(const kfc::model::Position& cell) {
    return PixelPoint{cell.col * kfc::input::kCellSizePixels, cell.row * kfc::input::kCellSizePixels};
}

}  // namespace kfc::graphics
