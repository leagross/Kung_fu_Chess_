#include "../../../../include/kfc/graphics/rendering/game_over_banner.hpp"

#include <string>

namespace kfc::graphics {

namespace {
constexpr double kFontScale = 1.4;
constexpr double kIntroFontScale = 1.8;
constexpr double kCountdownNumberScale = 3.0;
constexpr int kThickness = 3;

// Draws text horizontally centred on the board at vertical position y.
void put_centered(const std::string& text, int board_pixel_width, int y, double scale, Img& board_image) {
    int baseline = 0;
    cv::Size size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, scale, kThickness, &baseline);
    int x = (board_pixel_width - size.width) / 2;
    board_image.put_text(text, x, y, scale, cv::Scalar(255, 255, 255, 255), kThickness);
}
}  // namespace

void draw_game_over_banner(std::optional<kfc::model::PieceColor> winner, int board_pixel_width,
                            int board_pixel_height, Img& board_image) {
    Img overlay = Img::blank(board_pixel_width, board_pixel_height, cv::Scalar(0, 0, 0, 170));
    overlay.draw_on(board_image, 0, 0);

    std::string text = !winner.has_value()             ? "DRAW"
                        : (*winner == kfc::model::PieceColor::White) ? "WHITE WINS"
                                                                      : "BLACK WINS";

    int baseline = 0;
    cv::Size text_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, kFontScale, kThickness, &baseline);
    int x = (board_pixel_width - text_size.width) / 2;
    int y = (board_pixel_height + text_size.height) / 2;

    board_image.put_text(text, x, y, kFontScale, cv::Scalar(255, 255, 255, 255), kThickness);
}

void draw_intro_banner(int board_pixel_width, int board_pixel_height, double opacity, Img& board_image) {
    if (opacity <= 0.0) {
        return;
    }

    // Overlay and title both scale with opacity so the splash fades as one.
    int overlay_alpha = static_cast<int>(opacity * 160.0);
    Img overlay = Img::blank(board_pixel_width, board_pixel_height, cv::Scalar(0, 0, 0, overlay_alpha));
    overlay.draw_on(board_image, 0, 0);

    const std::string text = "KUNG FU CHESS";
    int baseline = 0;
    cv::Size text_size = cv::getTextSize(text, cv::FONT_HERSHEY_SIMPLEX, kIntroFontScale, kThickness, &baseline);
    int x = (board_pixel_width - text_size.width) / 2;
    int y = (board_pixel_height + text_size.height) / 2;

    int text_alpha = static_cast<int>(opacity * 255.0);
    board_image.put_text(text, x, y, kIntroFontScale, cv::Scalar(255, 255, 255, text_alpha), kThickness);
}

void draw_countdown_banner(int seconds_remaining, int board_pixel_width, int board_pixel_height, Img& board_image) {
    Img overlay = Img::blank(board_pixel_width, board_pixel_height, cv::Scalar(0, 0, 0, 140));
    overlay.draw_on(board_image, 0, 0);

    // Label above centre, the big seconds count below it.
    put_centered("OPPONENT LEFT", board_pixel_width, board_pixel_height / 2 - board_pixel_height / 12, kFontScale,
                 board_image);
    put_centered(std::to_string(seconds_remaining), board_pixel_width,
                 board_pixel_height / 2 + board_pixel_height / 6, kCountdownNumberScale, board_image);
}

void draw_searching_banner(int board_pixel_width, int board_pixel_height, Img& board_image) {
    Img overlay = Img::blank(board_pixel_width, board_pixel_height, cv::Scalar(0, 0, 0, 150));
    overlay.draw_on(board_image, 0, 0);
    put_centered("SEARCHING FOR OPPONENT", board_pixel_width, board_pixel_height / 2, kFontScale, board_image);
}

}  // namespace kfc::graphics
