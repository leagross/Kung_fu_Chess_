#pragma once

#include <string>

#include "kfc/graphics/primitives/img.hpp"
#include "kfc/realtime/move_log_observer.hpp"
#include "kfc/realtime/score_observer.hpp"

namespace kfc::graphics {

/// Draws [white panel][board][black panel]: white's moves on top/score at bottom, black's reversed.
class HudRenderer {
public:
    /// Empty username falls back to "White"/"Black"; rating 0 omits the parenthetical.
    void draw(const kfc::model::MoveLogObserver& move_log, const kfc::model::ScoreObserver& score,
              int board_pixel_width, int board_pixel_height, Img& canvas, const std::string& white_username = {},
              const std::string& black_username = {}, int white_rating = 0, int black_rating = 0) const;
};

}  // namespace kfc::graphics
