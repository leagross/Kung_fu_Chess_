#pragma once

#include "kfc/graphics/primitives/img.hpp"
#include "kfc/realtime/move_log_observer.hpp"
#include "kfc/realtime/score_observer.hpp"

namespace kfc::graphics {

/// Draws two side panels (each kHudPanelWidthPixels wide) onto a canvas
/// shaped [white panel][board][black panel]: white's on the left (moves on
/// top, score at the bottom) and black's on the right (score on top, moves
/// below) -- board_pixel_width/height describe the board region between
/// them. Only ever reads move_log/score -- it has no idea how they got
/// populated (Game notifying them as an Observer, once per arrival, is a
/// completely separate concern from drawing whatever they currently hold).
class HudRenderer {
public:
    void draw(const kfc::model::MoveLogObserver& move_log, const kfc::model::ScoreObserver& score,
              int board_pixel_width, int board_pixel_height, Img& canvas) const;
};

}  // namespace kfc::graphics
