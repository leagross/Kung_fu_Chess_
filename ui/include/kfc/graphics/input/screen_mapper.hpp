#pragma once

#include <string>

#include "kfc/graphics/geometry/board_geometry.hpp"

namespace kfc::graphics {

/// Maps a mouse position in the window's resizable on-screen size to fixed native canvas pixels.
class ScreenMapper {
public:
    /// window_name must already exist (cv::namedWindow called first).
    ScreenMapper(std::string window_name, int canvas_width, int canvas_height);

    /// Falls back to returning the input unchanged if the window size can't be queried.
    PixelPoint to_canvas_pixels(int display_x, int display_y) const;

private:
    std::string window_name_;
    int canvas_width_;
    int canvas_height_;
};

}  // namespace kfc::graphics
