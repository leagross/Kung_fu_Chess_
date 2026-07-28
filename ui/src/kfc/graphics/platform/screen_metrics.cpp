// Screen size and DPI awareness -- the last two OS-specific calls the client
// makes. Both are used only to choose a starting window size, so the non-
// Windows path returning a fixed default costs nothing: the window is
// resizable, and every layout decision downstream reads the window's actual
// size each frame (see the render loop's getWindowImageRect).
//
// This is the one file in ui/ that still branches on the platform, and it does
// so in a single #ifdef rather than scattering <windows.h> through the client.

#include "kfc/graphics/platform/screen_metrics.hpp"

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace kfc::graphics::platform {

ScreenSize prepare_display_and_measure_screen() {
#if defined(_WIN32)
    // Without this, on a scaled (high-DPI) display the window is created and
    // measured in logical pixels while HighGUI renders and reports
    // getWindowImageRect in physical ones -- so the per-frame window-sized
    // background covers only part of the actual window.
    SetProcessDPIAware();
    return {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
#else
    // A conservative 1080p assumption. Too small merely means the window opens
    // smaller than it could; the user can drag it larger, and nothing about the
    // rendering depends on this number being right.
    return {1920, 1080};
#endif
}

}  // namespace kfc::graphics::platform
