#pragma once

#include <string>

#include "kfc/graphics/geometry/board_geometry.hpp"

namespace kfc::graphics {

/// Converts a mouse event's raw pixel position (in the window's current,
/// resizable on-screen size) into fixed native canvas pixel space, scaled
/// uniformly to fit and centered. Queries the window's size at every click.
class ScreenMapper {
public:
    /// window_name must already exist (cv::namedWindow called first).
    /// canvas_width/height are the fixed size everything is actually drawn
    /// at (Img::blank(canvas_width, canvas_height, ...) in main, not
    /// whatever size the window happens to be on screen).
    ScreenMapper(std::string window_name, int canvas_width, int canvas_height);

    /// Rescales (display_x, display_y) from the window's current displayed
    /// size to canvas pixel space. Falls back to returning the input
    /// unchanged if the window's current size can't be queried.
    PixelPoint to_canvas_pixels(int display_x, int display_y) const;

private:
    std::string window_name_;
    int canvas_width_;
    int canvas_height_;
};

}  // namespace kfc::graphics
