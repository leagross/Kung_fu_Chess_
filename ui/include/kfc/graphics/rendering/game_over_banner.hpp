#pragma once

#include <optional>

#include "kfc/graphics/primitives/img.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Draws a semi-transparent dark overlay across the board area with "WHITE
/// WINS" / "BLACK WINS" / "DRAW" (winner == std::nullopt) centered on top.
void draw_game_over_banner(std::optional<kfc::model::PieceColor> winner, int board_pixel_width,
                            int board_pixel_height, Img& board_image);

/// A brief "KUNG FU CHESS" splash over the board at the given opacity (1.0
/// visible, 0.0 gone); a no-op at opacity <= 0.
void draw_intro_banner(int board_pixel_width, int board_pixel_height, double opacity, Img& board_image);

/// "OPPONENT LEFT" with seconds remaining, centered over the board, during a
/// dropped opponent's grace period.
void draw_countdown_banner(int seconds_remaining, int board_pixel_width, int board_pixel_height, Img& board_image);

/// A "waiting for opponent" overlay during networked matchmaking.
void draw_searching_banner(int board_pixel_width, int board_pixel_height, Img& board_image);

}  // namespace kfc::graphics
