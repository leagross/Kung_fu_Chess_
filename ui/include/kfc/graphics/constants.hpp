#pragma once

#include <filesystem>
#include <string>

namespace kfc::graphics {

/// KFC_GRAPHICS_ASSETS_DIR is injected by CMakeLists.txt from this source tree's location.
inline std::filesystem::path assets_root() {
    return KFC_GRAPHICS_ASSETS_DIR;
}

inline constexpr const char* kDefaultAssetPackName = "pieces";

inline constexpr const char* kBoardImageFilename = "board.png";

/// e.g. <pack>/<piece folder>/states/idle/sprites/1.png.
inline constexpr const char* kStatesFolderName = "states";
inline constexpr const char* kSpritesFolderName = "sprites";

inline constexpr const char* kIdleStateName = "idle";

/// State config fields: next state, frames_per_sec, is_loop.
inline constexpr const char* kStateConfigFilename = "config.json";

inline constexpr int kDefaultSpriteFrameIndex = 1;

inline std::string sprite_frame_filename(int frame_index) {
    return std::to_string(frame_index) + ".png";
}

/// In kfc::io::BoardParser's grammar.
inline constexpr const char* kDefaultBoardFilename = "default_board.txt";

/// KFC_GUI_APP_DIR is injected by CMake.
inline std::filesystem::path default_board_file() {
    return std::filesystem::path(KFC_GUI_APP_DIR) / kDefaultBoardFilename;
}

inline constexpr int kHudPanelWidthPixels = 180;

}  // namespace kfc::graphics
