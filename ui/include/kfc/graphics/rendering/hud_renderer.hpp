#pragma once

#include "kfc/graphics/primitives/img.hpp"
#include "kfc/realtime/move_log_observer.hpp"
#include "kfc/realtime/score_observer.hpp"

namespace kfc::graphics {

/// Draws two side panels (each kHudPanelWidthPixels wide) onto a canvas
/// shaped [white panel][board][black panel]: white's on the left (moves on
/// top, score at bottom), black's on the right (score on top, moves below).
class HudRenderer {
public:
    void draw(const kfc::model::MoveLogObserver& move_log, const kfc::model::ScoreObserver& score,
              int board_pixel_width, int board_pixel_height, Img& canvas) const;
};

}  // namespace kfc::graphics
