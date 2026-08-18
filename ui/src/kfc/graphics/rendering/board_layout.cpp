#include "../../../../include/kfc/graphics/rendering/board_layout.hpp"

#include <cmath>

#include "kfc/graphics/constants.hpp"

namespace kfc::graphics {

namespace {
constexpr int kBoardSourceWidth = 980;
constexpr int kBoardSourceHeight = 961;
constexpr int kBoardSourceGridInsetLeft = 116;
constexpr int kBoardSourceGridInsetTop = 116;
constexpr int kBoardSourceGridWidth = 748;
constexpr int kBoardSourceGridHeight = 736;
// Background visible around the framed board once it's centered on
// background.png -- a layout/pacing choice for this app, tune freely.
constexpr int kBoardBackgroundMarginPixels = 50;
}  // namespace

BoardLayout compute_board_layout(int board_pixel_width, int board_pixel_height) {
    // Scale board.png so its inner grid (not the full image) comes out to
    // exactly board_pixel_width x board_pixel_height.
    double board_scale_x = static_cast<double>(board_pixel_width) / kBoardSourceGridWidth;
    double board_scale_y = static_cast<double>(board_pixel_height) / kBoardSourceGridHeight;

    BoardLayout layout;
    layout.framed_board_width = static_cast<int>(std::lround(kBoardSourceWidth * board_scale_x));
    layout.framed_board_height = static_cast<int>(std::lround(kBoardSourceHeight * board_scale_y));
    layout.framed_board_inset_x = static_cast<int>(std::lround(kBoardSourceGridInsetLeft * board_scale_x));
    layout.framed_board_inset_y = static_cast<int>(std::lround(kBoardSourceGridInsetTop * board_scale_y));

    // Background fills the entire canvas (panels included), not just the
    // board's own column.
    layout.board_column_width = layout.framed_board_width + 2 * kBoardBackgroundMarginPixels;
    layout.board_column_height = layout.framed_board_height + 2 * kBoardBackgroundMarginPixels;
    layout.board_offset_x = kHudPanelWidthPixels;
    layout.canvas_width = kHudPanelWidthPixels * 2 + layout.board_column_width;
    layout.canvas_height = layout.board_column_height;

    layout.framed_board_x = layout.board_offset_x + kBoardBackgroundMarginPixels;
    layout.framed_board_y = kBoardBackgroundMarginPixels;
    layout.grid_offset_x = layout.framed_board_x + layout.framed_board_inset_x;
    layout.grid_offset_y = layout.framed_board_y + layout.framed_board_inset_y;

    return layout;
}

}  // namespace kfc::graphics
