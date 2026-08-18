#pragma once

#include <filesystem>
#include <string>

namespace kfc::graphics {

/// Root directory holding every asset pack. Value comes from
/// KFC_GRAPHICS_ASSETS_DIR, injected by CMakeLists.txt from this source
/// tree's location. A function rather than a global so the path is only
/// built when needed.
inline std::filesystem::path assets_root() {
    return KFC_GRAPHICS_ASSETS_DIR;
}

/// Folder name (under KFC_GRAPHICS_ASSETS_DIR) of the asset pack wired in.
inline constexpr const char* kDefaultAssetPackName = "pieces";

/// Filename of the board background texture inside an asset pack folder.
inline constexpr const char* kBoardImageFilename = "board.png";

/// Path segments inside a piece's folder down to a state's sprite frames,
/// e.g. <pack>/<piece folder>/states/idle/sprites/1.png.
inline constexpr const char* kStatesFolderName = "states";
inline constexpr const char* kSpritesFolderName = "sprites";

/// Starting animation state before any motion or cooldown begins.
inline constexpr const char* kIdleStateName = "idle";

/// Filename (under a state folder) of that state's config: next state,
/// frames_per_sec, is_loop.
inline constexpr const char* kStateConfigFilename = "config.json";

/// The frame index an animation starts from and, if non-looping, rests on
/// once finished.
inline constexpr int kDefaultSpriteFrameIndex = 1;

/// Builds a sprite frame's filename from its 1-based frame number, e.g.
/// sprite_frame_filename(1) == "1.png".
inline std::string sprite_frame_filename(int frame_index) {
    return std::to_string(frame_index) + ".png";
}

/// Filename (under KFC_GUI_APP_DIR) of the board-layout text file read at
/// startup, in kfc::io::BoardParser's grammar.
inline constexpr const char* kDefaultBoardFilename = "default_board.txt";

/// Path to that board-layout file; KFC_GUI_APP_DIR is injected by CMake.
inline std::filesystem::path default_board_file() {
    return std::filesystem::path(KFC_GUI_APP_DIR) / kDefaultBoardFilename;
}

/// Width, in pixels, of the side panel HudRenderer draws score/move-list
/// text into.
inline constexpr int kHudPanelWidthPixels = 180;

}  // namespace kfc::graphics
