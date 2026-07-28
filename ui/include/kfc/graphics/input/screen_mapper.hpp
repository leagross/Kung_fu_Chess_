#pragma once

#include <string>

#include "kfc/graphics/geometry/board_geometry.hpp"

namespace kfc::graphics {

/// Converts a mouse event's raw pixel position (in whatever size the OS
/// window is *currently displayed at*, which changes live as the user
/// drags a resizable cv::WINDOW_NORMAL window) into native canvas pixel
/// space -- the fixed width/height everything is actually drawn at,
/// regardless of window size. The canvas (board, pieces, HUD) is never
/// stretched out of proportion or cropped -- it's scaled uniformly to fit
/// inside the window and centered there; the background behind it is a
/// separate layer, scaled independently to cover the window completely
/// (see main.cpp's render loop) -- only the canvas's own placement matters
/// here, since only it receives clicks.
/// Queries the window's current on-screen size via cv::getWindowImageRect
/// at every click, since with a resizable window there is no other way to
/// know it. NOTE: this exact mechanism was previously found unreliable
/// with this project's vendored OpenCV 4.5.1 build (wrong click positions
/// after a drag-resize) -- re-enabled because resizing was explicitly
/// requested again, but verify clicks land on the right cell after
/// actually resizing the window, not just at its initial size.
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
