#pragma once

namespace kfc::graphics {

/// Immutable pixel-geometry for the app's window/canvas composition, shared by every consumer.
struct BoardLayout {
    /// board.png (frame included), scaled so its inner grid matches the playable board exactly.
    int framed_board_width;
    int framed_board_height;
    int framed_board_inset_x;
    int framed_board_inset_y;

    /// [background margin][framed board][background margin], as one column.
    int board_column_width;
    int board_column_height;

    /// [white HUD panel][background + framed board, centered][black HUD panel].
    int board_offset_x;
    int canvas_width;
    int canvas_height;

    /// Framed board's and playable grid's positions, in absolute canvas coordinates.
    int framed_board_x;
    int framed_board_y;
    int grid_offset_x;
    int grid_offset_y;
};

/// Rest is derived from board.png's fixed 980x961 size, 8x8 grid inset (116,116)-(864,852).
BoardLayout compute_board_layout(int board_pixel_width, int board_pixel_height);

}  // namespace kfc::graphics
