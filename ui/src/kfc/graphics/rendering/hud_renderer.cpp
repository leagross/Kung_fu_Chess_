#include "../../../../include/kfc/graphics/rendering/hud_renderer.hpp"

#include <cstdio>

#include "kfc/graphics/constants.hpp"

namespace kfc::graphics {

namespace {
constexpr int kMarginPixels = 12;
constexpr int kLineHeightPixels = 26;
constexpr int kPanelInsetPixels = 6;
constexpr int kMoveColumnOffsetPixels = 90;
constexpr int kColumnWidthPixels = 140;
constexpr double kTableFontScale = 0.52;
const cv::Scalar kTextColor(255, 255, 255, 255);
const cv::Scalar kPanelColor(20, 15, 10, 90);           // BGR near-black, mostly see-through
const cv::Scalar kPanelHeaderColor(20, 140, 210, 255);  // BGR gold, opaque

/// A translucent panel spanning the full canvas height, giving the move
/// table/score a bounded "table" look against the background scene instead
/// of floating text -- plus a thin gold rule under the header row.
void draw_panel(Img& canvas, int x, int width, int canvas_height) {
    Img panel = Img::blank(width - kPanelInsetPixels, canvas_height - 2 * kPanelInsetPixels, kPanelColor);
    panel.draw_on(canvas, x + kPanelInsetPixels / 2, kPanelInsetPixels);

    Img rule = Img::blank(width - kPanelInsetPixels, 2, kPanelHeaderColor);
    rule.draw_on(canvas, x + kPanelInsetPixels / 2, kMarginPixels + kLineHeightPixels + kPanelInsetPixels);
}

/// "mm:ss.mmm" -- RealTimeArbiter's own simulated clock (the same one
/// ArrivalEvent::arrived_at_ms carries), not wall-clock time.
std::string format_time_ms(long long ms) {
    long long minutes = ms / 60000;
    long long seconds = (ms / 1000) % 60;
    long long millis = ms % 1000;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02lld:%02lld.%03lld", minutes, seconds, millis);
    return buf;
}

/// Draws a "Time | Move" table starting at (x, y), wrapping into a new
/// column (its own "Time | Move" header repeated) once the current one
/// fills the vertical space down to canvas_height, instead of ever growing
/// past the panel or silently capping at a fixed row count -- new columns
/// stop once panel_right_x is reached, at which point only the most
/// recent entries that still fit (across every column together) are kept,
/// oldest-first within each column, left column oldest.
void draw_move_table(Img& image, const std::vector<kfc::model::MoveLogEntry>& entries, int x, int y,
                      int canvas_height, int panel_right_x) {
    int rows_per_column = std::max(1, (canvas_height - y - kMarginPixels) / kLineHeightPixels - 1);
    int max_columns = std::max(1, (panel_right_x - x) / kColumnWidthPixels);
    std::size_t capacity = static_cast<std::size_t>(rows_per_column) * static_cast<std::size_t>(max_columns);
    std::size_t first = entries.size() > capacity ? entries.size() - capacity : 0;

    int column_x = x;
    int row_y = y;
    int rows_in_column = 0;
    auto draw_header = [&]() {
        image.put_text("Time", column_x, row_y, kTableFontScale, kTextColor);
        image.put_text("Move", column_x + kMoveColumnOffsetPixels, row_y, kTableFontScale, kTextColor);
        row_y += kLineHeightPixels;
    };
    draw_header();

    for (std::size_t i = first; i < entries.size(); ++i) {
        if (rows_in_column >= rows_per_column) {
            column_x += kColumnWidthPixels;
            row_y = y;
            rows_in_column = 0;
            draw_header();
        }
        image.put_text(format_time_ms(entries[i].time_ms), column_x, row_y, kTableFontScale, kTextColor);
        image.put_text(entries[i].notation, column_x + kMoveColumnOffsetPixels, row_y, kTableFontScale, kTextColor);
        row_y += kLineHeightPixels;
        ++rows_in_column;
    }
}
}  // namespace

void HudRenderer::draw(const kfc::model::MoveLogObserver& move_log, const kfc::model::ScoreObserver& score,
                        int board_pixel_width, int board_pixel_height, Img& canvas) const {
    int white_x = kMarginPixels;
    int black_x = kHudPanelWidthPixels + board_pixel_width + kMarginPixels;
    int canvas_height = board_pixel_height;

    draw_panel(canvas, 0, kHudPanelWidthPixels, canvas_height);
    draw_panel(canvas, kHudPanelWidthPixels + board_pixel_width, kHudPanelWidthPixels, canvas_height);

    // White (left): header, then score, then the table grows downward
    // below both -- mirroring Black's layout on the other side.
    int y = kMarginPixels + kLineHeightPixels;
    canvas.put_text("White score: " + std::to_string(score.score(kfc::model::PieceColor::White)), white_x, y, 0.7,
                     kTextColor);
    y += kLineHeightPixels * 2;
    canvas.put_text("White", white_x, y, 0.65, kTextColor);
    y += kLineHeightPixels;
    draw_move_table(canvas, move_log.entries(kfc::model::PieceColor::White), white_x, y, canvas_height,
                     kHudPanelWidthPixels);

    // Black (right): score first, the table grows downward below it.
    y = kMarginPixels + kLineHeightPixels;
    canvas.put_text("Black score: " + std::to_string(score.score(kfc::model::PieceColor::Black)), black_x, y, 0.7,
                     kTextColor);
    y += kLineHeightPixels * 2;
    canvas.put_text("Black", black_x, y, 0.65, kTextColor);
    y += kLineHeightPixels;
    int canvas_width = kHudPanelWidthPixels * 2 + board_pixel_width;
    draw_move_table(canvas, move_log.entries(kfc::model::PieceColor::Black), black_x, y, canvas_height, canvas_width);
}

}  // namespace kfc::graphics
