#include "kfc/graphics/app/home_screen.hpp"

#include <algorithm>

#include <opencv2/opencv.hpp>

namespace kfc::graphics::app {

namespace {

using kfc::graphics::dialogs::RoomChoice;

// Records the most recent left-click, for the home-screen buttons.
struct HomeClick {
    int x = -1;
    int y = -1;
    bool clicked = false;
};

}  // namespace

Button draw_button(Img& frame, const std::string& label, int cx, int cy, int w, int h) {
    Button button{cx - w / 2, cy - h / 2, w, h};
    Img::blank(w, h, cv::Scalar(220, 220, 220, 255)).draw_on(frame, button.x, button.y);
    Img::blank(w - 6, h - 6, cv::Scalar(30, 30, 30, 255)).draw_on(frame, button.x + 3, button.y + 3);

    double font = h / 45.0;
    int baseline = 0;
    cv::Size text = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, font, 3, &baseline);
    frame.put_text(label, button.x + (w - text.width) / 2, button.y + (h + text.height) / 2, font,
                   cv::Scalar(255, 255, 255, 255), 3);
    return button;
}

std::optional<kfc::protocol::ClientMessage> run_home_screen(const std::string& window_name,
                                                            const Img& background_source,
                                                            dialogs::IRoomPrompt& prompt) {
    HomeClick click;
    cv::setMouseCallback(
        window_name,
        [](int event, int x, int y, int, void* userdata) {
            if (event == cv::EVENT_LBUTTONDOWN) {
                auto* recorded = static_cast<HomeClick*>(userdata);
                recorded->x = x;
                recorded->y = y;
                recorded->clicked = true;
            }
        },
        &click);

    // Detach the callback from `click` (a local) before returning, so nothing
    // dereferences it during the connect() that follows.
    auto finish = [&window_name](std::optional<kfc::protocol::ClientMessage> result) {
        cv::setMouseCallback(window_name, [](int, int, int, int, void*) {}, nullptr);
        return result;
    };

    while (true) {
        cv::Rect rect = cv::getWindowImageRect(window_name);
        int width = rect.width > 0 ? rect.width : 960;
        int height = rect.height > 0 ? rect.height : 720;

        Img frame = background_source.cover_scaled(width, height);
        int button_width = std::max(200, width / 4);
        int button_height = std::max(64, height / 11);
        Button play = draw_button(frame, "PLAY", width / 2, height / 2 - button_height, button_width, button_height);
        Button room = draw_button(frame, "ROOM", width / 2, height / 2 + button_height, button_width, button_height);

        cv::imshow(window_name, frame.get_mat());
        if (cv::waitKey(16) == 27) {  // Esc
            return finish(std::nullopt);
        }
        if (cv::getWindowProperty(window_name, cv::WND_PROP_VISIBLE) < 1.0) {
            return finish(std::nullopt);  // window closed
        }
        if (!click.clicked) {
            continue;
        }

        click.clicked = false;
        if (play.hit(click.x, click.y)) {
            return finish(kfc::protocol::Play{});
        }
        if (!room.hit(click.x, click.y)) {
            continue;
        }

        RoomChoice choice = prompt.ask_room();
        if (choice.action == RoomChoice::Action::Create) {
            // No name: the server generates one and reports it in Welcome.
            return finish(kfc::protocol::CreateRoom{});
        }
        if (choice.action == RoomChoice::Action::Join && !choice.room_id.empty()) {
            return finish(kfc::protocol::JoinRoom{choice.room_id});
        }
        // Cancel, or Join with nothing typed -> stay on the home screen.
    }
}

}  // namespace kfc::graphics::app
