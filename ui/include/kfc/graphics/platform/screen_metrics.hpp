#pragma once

namespace kfc::graphics::platform {

/// A platform with no way to ask returns a sensible default.
struct ScreenSize {
    int width;
    int height;
};

/// Also makes the process DPI-aware on Windows. Call once, before creating any window.
[[nodiscard]] ScreenSize prepare_display_and_measure_screen();

}  // namespace kfc::graphics::platform
