#pragma once

namespace kfc::graphics::platform {

/// The desktop's size in physical pixels, used only to pick a starting window
/// size that fits the screen. The window is resizable afterwards, so a rough
/// answer is fine -- a platform with no way to ask simply returns a sensible
/// default rather than failing.
struct ScreenSize {
    int width;
    int height;
};

/// Asks the OS how big the screen is. Also makes the process DPI-aware where
/// that is a thing (Windows), because otherwise the window is created and
/// measured in logical pixels while the renderer works in physical ones, and
/// the background ends up covering only part of the window.
///
/// Call once, before creating any window.
[[nodiscard]] ScreenSize prepare_display_and_measure_screen();

}  // namespace kfc::graphics::platform
