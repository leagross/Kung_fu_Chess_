#pragma once

#include <optional>

#include "kfc/graphics/primitives/img.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics {

/// Draws a semi-transparent dark overlay across the board area (not the HUD
/// panel) with "WHITE WINS" / "BLACK WINS" / "DRAW" (when winner is
/// std::nullopt) centered on top. A free function, not a class: nothing here
/// needs to remember anything between frames -- the caller (driven by a
/// GameEnded event off the game's bus) owns the one piece of state (who won, or
/// that it was a draw) that matters, this only ever draws whatever it's told.
void draw_game_over_banner(std::optional<kfc::model::PieceColor> winner, int board_pixel_width,
                            int board_pixel_height, Img& board_image);

/// The start-of-game counterpart: a brief "KUNG FU CHESS" splash over the board
/// at the given opacity (1.0 = fully visible, 0.0 = gone), which the caller
/// ramps down over the intro window so the splash fades out as play begins.
/// Driven by a GameStarted event off the game's bus. A no-op at opacity <= 0.
void draw_intro_banner(int board_pixel_width, int board_pixel_height, double opacity, Img& board_image);

/// A mid-game overlay while a dropped opponent's grace period counts down:
/// "OPPONENT LEFT" with the seconds remaining, centered over the board. Driven
/// by an OpponentCountdown event off the bus (networked play only); a GameEnded
/// takes over if the opponent never returns.
void draw_countdown_banner(int seconds_remaining, int board_pixel_width, int board_pixel_height, Img& board_image);

/// A "waiting for opponent" overlay shown while a networked Play search is in
/// progress -- seated in a room but no rating-compatible opponent matched in
/// yet (see GameSession::is_match_started).
void draw_searching_banner(int board_pixel_width, int board_pixel_height, Img& board_image);

}  // namespace kfc::graphics
