#pragma once

namespace kfc::graphics {

/// Every pixel-geometry constant the app's window/canvas composition
/// depends on, derived once from board.png's own known dimensions and the
/// playable board's pixel size -- see compute_board_layout's own comment
/// for where the board.png numbers come from. Kept as one immutable value
/// (not recomputed piecemeal) so every consumer (asset loading, the render
/// loop) agrees on the same layout.
struct BoardLayout {
    /// board.png (frame included), scaled so its inner grid matches the
    /// playable board's own pixel size exactly.
    int framed_board_width;
    int framed_board_height;
    /// Where the playable grid starts inside the scaled, framed board.png.
    int framed_board_inset_x;
    int framed_board_inset_y;

    /// [background margin][framed board][background margin], as one column.
    int board_column_width;
    int board_column_height;

    /// [white HUD panel][background + framed board, centered][black HUD
    /// panel] -- the whole canvas this app composes and renders every frame.
    int board_offset_x;
    int canvas_width;
    int canvas_height;

    /// Where the framed board sits on the canvas, and where the playable
    /// grid sits within that -- both in absolute canvas coordinates.
    int framed_board_x;
    int framed_board_y;
    int grid_offset_x;
    int grid_offset_y;
};

/// board_pixel_width/height is the playable grid's own size (board width/
/// height in cells times kCellSizePixels) -- everything else in BoardLayout
/// is derived from that plus board.png's fixed, measured-by-hand dimensions
/// (a 980x961 image: an 8x8 marble grid inset inside its own decorative
/// frame, running from (116,116) to (864,852) in that specific file -- a
/// different board.png would need its own numbers, this is not a generic
/// asset-pack mechanism).
BoardLayout compute_board_layout(int board_pixel_width, int board_pixel_height);

}  // namespace kfc::graphics
