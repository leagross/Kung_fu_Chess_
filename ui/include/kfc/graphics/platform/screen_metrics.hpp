#pragma once

namespace kfc::graphics::platform {

/// The desktop's size in physical pixels, used to pick a starting window
/// size. A platform with no way to ask returns a sensible default.
struct ScreenSize {
    int width;
    int height;
};

/// Asks the OS how big the screen is, and makes the process DPI-aware on
/// Windows so the renderer's physical pixels match the window's logical
/// ones. Call once, before creating any window.
[[nodiscard]] ScreenSize prepare_display_and_measure_screen();

}  // namespace kfc::graphics::platform
