#pragma once

#include <optional>
#include <string>

#include "kfc/graphics/dialogs/room_prompt.hpp"
#include "kfc/graphics/primitives/img.hpp"
#include "kfc/protocol/messages.hpp"

namespace kfc::graphics::app {

/// A rectangle that knows whether a click landed in it.
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
/// callers test clicks against exactly what was drawn.
Button draw_button(Img& frame, const std::string& label, int cx, int cy, int w, int h);

/// The first screen of a networked client: PLAY or ROOM. Blocks until the
/// user picks something, returning the seating message to send, or
/// std::nullopt if they closed the window. Draws into main()'s own window.
[[nodiscard]] std::optional<kfc::protocol::ClientMessage> run_home_screen(const std::string& window_name,
                                                                          const Img& background_source,
                                                                          dialogs::IRoomPrompt& prompt);

}  // namespace kfc::graphics::app
