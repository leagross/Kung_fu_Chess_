#pragma once

#include <string>

#include "kfc/graphics/primitives/img.hpp"
#include "kfc/realtime/move_log_observer.hpp"
#include "kfc/realtime/score_observer.hpp"

namespace kfc::graphics {

/// Draws two side panels (each kHudPanelWidthPixels wide) onto a canvas
/// shaped [white panel][board][black panel]: white's on the left (moves on
/// top, score at bottom), black's on the right (score on top, moves below).
class HudRenderer {
public:
    /// white_username/black_username label each panel (UI spec: "Presenting
    /// player names") -- empty falls back to the plain colour name "White"/
    /// "Black", which covers local play (no accounts at all) and the brief
    /// window before White's own MatchStart names Black (see
    /// GameSession::black_username's own doc comment). white_rating/
    /// black_rating are shown alongside the name in parentheses; 0 (the
    /// same "not known yet" convention as the username) omits it.
    void draw(const kfc::model::MoveLogObserver& move_log, const kfc::model::ScoreObserver& score,
              int board_pixel_width, int board_pixel_height, Img& canvas, const std::string& white_username = {},
              const std::string& black_username = {}, int white_rating = 0, int black_rating = 0) const;
};

}  // namespace kfc::graphics
