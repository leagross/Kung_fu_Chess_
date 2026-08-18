#include "../../../../include/kfc/graphics/input/screen_mapper.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include <opencv2/opencv.hpp>

namespace kfc::graphics {

ScreenMapper::ScreenMapper(std::string window_name, int canvas_width, int canvas_height)
    : window_name_(std::move(window_name)), canvas_width_(canvas_width), canvas_height_(canvas_height) {
    if (canvas_width_ <= 0 || canvas_height_ <= 0) {
        throw std::invalid_argument("ScreenMapper: canvas width and height must be positive");
    }
}

PixelPoint ScreenMapper::to_canvas_pixels(int display_x, int display_y) const {
    cv::Rect displayed = cv::getWindowImageRect(window_name_);
    if (displayed.width <= 0 || displayed.height <= 0) {
        // Window not currently displayable (e.g. minimized): no valid mapping.
        // Return an off-board point so the click is rejected downstream.
        return PixelPoint{-1, -1};
    }

    // Scaling here must match main.cpp's render loop exactly, or clicks land
    // in the wrong place. lround plus clamping guards against float rounding
    // pushing the "fit" size a pixel past the window (negative offset).
    double scale = std::min(static_cast<double>(displayed.width) / canvas_width_,
                             static_cast<double>(displayed.height) / canvas_height_);
    int rendered_width = std::min(displayed.width, static_cast<int>(std::lround(canvas_width_ * scale)));
    int rendered_height = std::min(displayed.height, static_cast<int>(std::lround(canvas_height_ * scale)));
    int offset_x = std::max(0, (displayed.width - rendered_width) / 2);
    int offset_y = std::max(0, (displayed.height - rendered_height) / 2);

    return PixelPoint{
        static_cast<int>((display_x - offset_x) / scale),
        static_cast<int>((display_y - offset_y) / scale),
    };
}

}  // namespace kfc::graphics
