#pragma once

#include <optional>
#include <string>

#include "kfc/graphics/dialogs/room_prompt.hpp"
#include "kfc/graphics/primitives/img.hpp"
#include "kfc/protocol/messages.hpp"

namespace kfc::graphics::app {

/// A rectangle that knows whether a click landed in it. The home screen's two
/// buttons are the only clickable things this client draws itself -- everything
/// else on screen is the board, and ScreenMapper handles that.
struct Button {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    [[nodiscard]] bool hit(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

/// Draws a bordered button centred at (cx, cy) and returns its rectangle, so
/// the caller can test clicks against exactly what was drawn rather than
/// against a second, separately computed copy of the same geometry.
Button draw_button(Img& frame, const std::string& label, int cx, int cy, int w, int h);

/// The first screen of a networked client: **PLAY** (matchmaking) and **ROOM**
/// (create or join a named one, via the platform's dialog).
///
/// Blocks until the user picks something, returning the seating message to send
/// once connected -- Play, CreateRoom or JoinRoom -- or std::nullopt if they
/// closed the window or pressed Esc. Cancelling the Room dialog, or pressing
/// Join with nothing typed, simply returns to the home screen rather than
/// counting as a choice.
///
/// Draws into the window main() already created, so the game continues in that
/// same window rather than flashing a second one.
[[nodiscard]] std::optional<kfc::protocol::ClientMessage> run_home_screen(const std::string& window_name,
                                                                          const Img& background_source,
                                                                          dialogs::IRoomPrompt& prompt);

}  // namespace kfc::graphics::app
