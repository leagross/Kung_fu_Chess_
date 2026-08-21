#include "../../../../include/kfc/graphics/input/mouse_input_adapter.hpp"

#include <opencv2/opencv.hpp>

namespace kfc::graphics {

MouseInputAdapter::MouseInputAdapter(const std::string& window_name, kfc::texttests::IGameView& game,
                                      const ScreenMapper& screen_mapper, int board_offset_x, int board_offset_y)
    : game_(game), screen_mapper_(screen_mapper), board_offset_x_(board_offset_x), board_offset_y_(board_offset_y) {
    cv::setMouseCallback(window_name, &MouseInputAdapter::on_mouse_event, this);
}

void MouseInputAdapter::on_mouse_event(int event, int x, int y, int /*flags*/, void* userdata) {
    if (event != cv::EVENT_LBUTTONDBLCLK && event != cv::EVENT_LBUTTONDOWN) {
        return;
    }

    auto* adapter = static_cast<MouseInputAdapter*>(userdata);
    PixelPoint canvas_pixel = adapter->screen_mapper_.to_canvas_pixels(x, y);
    int board_x = canvas_pixel.x - adapter->board_offset_x_;
    int board_y = canvas_pixel.y - adapter->board_offset_y_;

    if (event == cv::EVENT_LBUTTONDBLCLK) {
        // The first press already reached click() below before this event
        // fires; that's harmless (own-piece click just selects, and any
        // other click's move request is unaffected by the jump that follows).
        adapter->game_.jump(board_x, board_y);
        return;
    }

    adapter->game_.click(board_x, board_y);
}

}  // namespace kfc::graphics
