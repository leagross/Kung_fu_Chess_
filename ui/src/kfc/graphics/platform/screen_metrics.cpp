// The one file in ui/ that still branches on platform.

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
    // Otherwise a scaled display measures the window in logical pixels while HighGUI renders physical ones.
    SetProcessDPIAware();
    return {GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
#else
    return {1920, 1080};  // conservative default; window is resizable anyway
#endif
}

}  // namespace kfc::graphics::platform
