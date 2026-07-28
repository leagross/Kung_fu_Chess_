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
        // The first press of a double-click already reached Game::click below
        // before this event fires. That preceding click is harmless in every
        // real case, which is why no debounce (and the ~double-click-time of
        // input latency it would add to *every* single click, bad for a
        // real-time game) is used here:
        //  - Jumping is done on one's *own* piece. Clicking an own piece only
        //    ever selects/reselects it (Controller never treats an own-colour
        //    cell as a move target), so no move is requested -- just a select,
        //    then this jump.
        //  - If the double-click lands anywhere else while a piece was
        //    selected, the preceding click requests exactly the move the user
        //    aimed at that cell, and this jump is then rejected (that cell
        //    holds no own piece to jump) -- no spurious extra action.
        // Controller::jump itself never reads or modifies the selection.
        adapter->game_.jump(board_x, board_y);
        return;
    }

    adapter->game_.click(board_x, board_y);
}

}  // namespace kfc::graphics
