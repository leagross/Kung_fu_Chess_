#pragma once

#include <string>

#include "kfc/graphics/input/screen_mapper.hpp"
#include "kfc/texttests/game_view.hpp"

namespace kfc::graphics {

/// Forwards left-clicks to IGameView::click and double-clicks to IGameView::jump, in board-local pixels.
class MouseInputAdapter {
public:
    /// game and screen_mapper must outlive this adapter; window_name must already exist.
    MouseInputAdapter(const std::string& window_name, kfc::texttests::IGameView& game,
                       const ScreenMapper& screen_mapper, int board_offset_x, int board_offset_y = 0);

private:
    static void on_mouse_event(int event, int x, int y, int flags, void* userdata);

    kfc::texttests::IGameView& game_;
    const ScreenMapper& screen_mapper_;
    int board_offset_x_;
    int board_offset_y_;
};

}  // namespace kfc::graphics
