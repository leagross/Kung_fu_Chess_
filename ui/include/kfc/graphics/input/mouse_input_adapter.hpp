#pragma once

#include <string>

#include "kfc/graphics/input/screen_mapper.hpp"
#include "kfc/texttests/game_view.hpp"

namespace kfc::graphics {

/// Owns the one piece of OS-facing state a window's mouse handling needs:
/// registering an OpenCV mouse callback and forwarding left-clicks to
/// IGameView::click and double-clicks to IGameView::jump. Isolates the one
/// non-portable, C-style-callback dependency (cv::setMouseCallback's void*
/// userdata idiom) so nothing else in the graphics layer has to know that
/// mechanism exists. Every raw event position passes through screen_mapper
/// first (undoes any window-resize stretching), then has board_offset_x/
/// board_offset_y subtracted (undoes the left HUD panel's width and, for a
/// board asset framed by its own decorative border, the margin/frame
/// inset above the playable grid) before reaching the game view --
/// Controller/BoardMapper only ever see board-local pixel coordinates,
/// exactly as before either of those existed. game is IGameView, not the
/// concrete Game, so the exact same adapter drives either local
/// single-player play or a networked ServerLink.
class MouseInputAdapter {
public:
    /// Registers this adapter as window_name's mouse callback. game and
    /// screen_mapper must outlive this MouseInputAdapter; window_name must
    /// already exist (cv::namedWindow called first). board_offset_y
    /// defaults to 0 for a board drawn flush against the canvas's top edge.
    MouseInputAdapter(const std::string& window_name, kfc::texttests::IGameView& game,
                       const ScreenMapper& screen_mapper, int board_offset_x, int board_offset_y = 0);

private:
    /// OpenCV's required free-function/static callback signature. userdata
    /// is always the MouseInputAdapter that registered it -- see the
    /// constructor.
    static void on_mouse_event(int event, int x, int y, int flags, void* userdata);

    kfc::texttests::IGameView& game_;
    const ScreenMapper& screen_mapper_;
    int board_offset_x_;
    int board_offset_y_;
};

}  // namespace kfc::graphics
