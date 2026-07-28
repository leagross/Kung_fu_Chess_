// Mouse-coordinate verification tool (not part of the real game). Draws a
// red marker at the live mouse position and a green marker at the last
// click, printing which BoardMapper cell (if any) each one resolves to --
// exactly the debugging step the graphics lecture recommended before
// trusting pixel-to-cell math inside Controller itself.
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>

#include <opencv2/opencv.hpp>

#include "../../include/kfc/graphics/constants.hpp"
#include "../../include/kfc/graphics/primitives/img.hpp"
#include "kfc/input/board_mapper.hpp"

namespace {

struct MouseState {
    int x = -1;
    int y = -1;
    bool has_click = false;
    int click_x = -1;
    int click_y = -1;
};

/// OpenCV's mouse callback signature -- called by cv::setMouseCallback on
/// every mouse event inside the window. userdata is whatever pointer we
/// registered it with (our MouseState), cast back here since OpenCV's C-style
/// callback API has no way to express "a MouseState*" in its own signature.
void on_mouse(int event, int x, int y, int /*flags*/, void* userdata) {
    auto* state = static_cast<MouseState*>(userdata);
    state->x = x;
    state->y = y;
    if (event == cv::EVENT_LBUTTONDOWN) {
        state->has_click = true;
        state->click_x = x;
        state->click_y = y;
    }
}

/// "(2,3)" for a cell inside the board, "out of board" for a pixel outside
/// it -- BoardMapper::pixel_to_cell returns std::nullopt for the latter.
std::string describe_cell(const kfc::input::BoardMapper& mapper, int x, int y) {
    std::optional<kfc::model::Position> cell = mapper.pixel_to_cell(x, y);
    return cell.has_value() ? kfc::model::to_string(*cell) : "out of board";
}

}  // namespace

int main() {
    try {
        int board_width = 8;
        int board_height = 8;
        int canvas_width = board_width * kfc::input::kCellSizePixels;
        int canvas_height = board_height * kfc::input::kCellSizePixels;

        std::filesystem::path board_path =
            kfc::graphics::assets_root() / kfc::graphics::kDefaultAssetPackName / kfc::graphics::kBoardImageFilename;

        kfc::graphics::Img base;
        base.read(board_path.string(), {canvas_width, canvas_height});

        kfc::input::BoardMapper mapper(board_width, board_height);

        MouseState mouse_state;
        const std::string window_name = "Image";
        cv::namedWindow(window_name);
        cv::setMouseCallback(window_name, on_mouse, &mouse_state);

        std::cout << "Move the mouse over the window; click to mark a cell. Press any key to quit.\n";

        while (true) {
            // A fresh deep copy every frame -- base.get_mat() is a reference
            // to Img's own pixel buffer; drawing markers must never mutate
            // that, or every frame would keep the previous frame's marker
            // baked in permanently.
            cv::Mat frame = base.get_mat().clone();

            cv::circle(frame, cv::Point(mouse_state.x, mouse_state.y), 5, cv::Scalar(0, 0, 255), -1);
            std::string hover_text = "mouse: " + describe_cell(mapper, mouse_state.x, mouse_state.y);
            cv::putText(frame, hover_text, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 0, 255), 2);

            if (mouse_state.has_click) {
                cv::circle(frame, cv::Point(mouse_state.click_x, mouse_state.click_y), 8, cv::Scalar(0, 255, 0), 2);
                std::string click_text = "last click: " + describe_cell(mapper, mouse_state.click_x, mouse_state.click_y);
                cv::putText(frame, click_text, cv::Point(10, 60), cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(0, 255, 0),
                            2);
            }

            cv::imshow(window_name, frame);
            // A short timeout (not waitKey(0)) so the loop keeps redrawing
            // and tracking mouse movement between key presses -- any key
            // returns a value >= 0 and ends the loop; no key within 30ms
            // returns -1 and we redraw the next frame.
            if (cv::waitKey(30) >= 0) {
                break;
            }
        }

        cv::destroyAllWindows();
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
