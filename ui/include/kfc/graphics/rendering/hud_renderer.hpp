#pragma once

#include <string>

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
    /// white_username/black_username label each panel (UI spec: "Presenting
    /// player names") -- empty falls back to the plain colour name "White"/
    /// "Black", which covers local play (no accounts at all) and the brief
    /// window before White's own MatchStart names Black (see
    /// GameSession::black_username's own doc comment).
    void draw(const kfc::model::MoveLogObserver& move_log, const kfc::model::ScoreObserver& score,
              int board_pixel_width, int board_pixel_height, Img& canvas, const std::string& white_username = {},
              const std::string& black_username = {}) const;
};

}  // namespace kfc::graphics
