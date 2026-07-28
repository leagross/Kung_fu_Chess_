#pragma once

#include <filesystem>
#include <string>

namespace kfc::graphics {

/// Root directory holding every asset pack (each pack is one subfolder:
/// board.png plus per-piece state folders). Value comes from
/// KFC_GRAPHICS_ASSETS_DIR, a macro CMakeLists.txt injects via
/// target_compile_definitions(kfc_gui_app ...) -- computed once by CMake from
/// this source tree's own location, so it stays correct on any machine that
/// builds the project, unlike a path typed into C++ by hand. A function
/// rather than a stored global so nothing builds a filesystem::path unless
/// it's actually asked for.
inline std::filesystem::path assets_root() {
    return KFC_GRAPHICS_ASSETS_DIR;
}

/// Folder name (directly under KFC_GRAPHICS_ASSETS_DIR) of the asset pack
/// currently wired into the app. Different packs may name their piece
/// folders differently -- see IPieceCodeScheme -- this is only "which pack",
/// not "how to read one".
inline constexpr const char* kDefaultAssetPackName = "pieces_mine";

/// Filename of the board background texture inside an asset pack folder.
/// Every pack (pieces1, pieces_mine) ships its own board.png at this name.
inline constexpr const char* kBoardImageFilename = "board.png";

/// Path segments inside one piece's folder down to a specific state's
/// sprite frames, e.g. <pack>/<piece folder>/states/idle/sprites/1.png.
/// Every asset pack shares this internal layout regardless of how it names
/// the piece folder itself (that part is IPieceCodeScheme's job, not this).
inline constexpr const char* kStatesFolderName = "states";
inline constexpr const char* kSpritesFolderName = "sprites";

/// Name of the resting/default animation state a piece starts in before any
/// motion or cooldown has begun. PieceAnimator now drives state transitions
/// (Move/Jump/rest/back to Idle) on its own -- this constant only names the
/// starting state, not "the one state this app draws".
inline constexpr const char* kIdleStateName = "idle";

/// Filename (directly under a state folder, alongside kSpritesFolderName) of
/// that state's config: physics.next_state_when_finished, graphics.frames_per_sec,
/// graphics.is_loop. (physics.speed_m_per_sec is still present in the files but
/// no longer read -- movement speed now comes from the backend's speed
/// provider, see kfc::model::kDefaultPieceSpeedProvider, so local and
/// networked play stay in sync.)
inline constexpr const char* kStateConfigFilename = "config.json";

/// The frame an animation starts from and, for a non-looping state, rests
/// on once finished -- e.g. the one frame this app currently draws for
/// idle. A number, not a filename: once PieceAnimator cycles through many
/// frames, every one of them is still "a frame index", and this is simply
/// which index is the resting/default one. Kept separate from
/// sprite_frame_filename() below so the *value* (which frame is default)
/// and the *format* (how a frame number becomes a filename) can each change
/// independently.
inline constexpr int kDefaultSpriteFrameIndex = 1;

/// Builds a sprite frame's filename from its 1-based frame number, e.g.
/// sprite_frame_filename(1) == "1.png". Every state's sprites folder (see
/// pieces_mine/*/states/*/sprites/) numbers its frames this way -- change
/// this one function if that format ever changes, instead of every call
/// site that names a frame.
inline std::string sprite_frame_filename(int frame_index) {
    return std::to_string(frame_index) + ".png";
}

/// Filename (directly under KFC_GUI_APP_DIR) of the board-layout text file
/// this app reads at startup, in kfc::io::BoardParser's own grammar (one
/// rank per line, "wK"/"bQ"/"." tokens). This is where WHAT the starting
/// position is lives -- not here. This constant only names the file, the
/// same way kBoardImageFilename only names a file rather than storing pixel
/// data.
inline constexpr const char* kDefaultBoardFilename = "default_board.txt";

/// Path to that board-layout file. Mirrors assets_root(): value comes from
/// KFC_GUI_APP_DIR, a second macro CMakeLists.txt injects the same way.
inline std::filesystem::path default_board_file() {
    return std::filesystem::path(KFC_GUI_APP_DIR) / kDefaultBoardFilename;
}

/// Width, in pixels, of the side panel HudRenderer draws score/move-list
/// text into -- reserved to the right of the board itself. A pacing/layout
/// choice for this app, the same kind of decision meters_per_cell in
/// main.cpp is, not something derivable from any asset pack.
inline constexpr int kHudPanelWidthPixels = 180;

}  // namespace kfc::graphics
