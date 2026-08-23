#pragma once

#include <optional>

#include "kfc/graphics/primitives/img.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// winner == std::nullopt draws "DRAW" instead of a color's win.
void draw_game_over_banner(std::optional<kfc::model::PieceColor> winner, int board_pixel_width,
                            int board_pixel_height, Img& board_image);

/// No-op at opacity <= 0.
void draw_intro_banner(int board_pixel_width, int board_pixel_height, double opacity, Img& board_image);

void draw_countdown_banner(int seconds_remaining, int board_pixel_width, int board_pixel_height, Img& board_image);

void draw_searching_banner(int board_pixel_width, int board_pixel_height, Img& board_image);

}  // namespace kfc::graphics
