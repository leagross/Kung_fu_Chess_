// The portable implementation of IRoomPrompt, for every platform that is not
// Windows. Compiled only there -- see CMakeLists and win_room_prompt.cpp.
//
// Drawn with OpenCV, which the client already depends on and which builds on
// Linux and macOS as readily as on Windows. That is the whole reason for
// choosing it over a native toolkit here: it adds no new dependency at all.
//
// The cost is text entry. Win32 gives an EDIT control that handles typing,
// caret, selection and clipboard for free; here the room id has to be
// assembled key by key from cv::waitKey, which is why this file has an input
// loop and the Windows one does not. Room ids are four characters from a
// deliberately unambiguous alphabet (see RoomManager::generate_room_id), so
// that is a fair trade -- but if a richer dialog is ever wanted on Linux, this
// is the file to replace with tinyfiledialogs or SDL.

#include "kfc/graphics/dialogs/room_prompt.hpp"

#include <algorithm>
#include <cctype>

#include <opencv2/opencv.hpp>

namespace kfc::graphics::dialogs {

namespace {

constexpr int kWidth = 460;
constexpr int kHeight = 220;
constexpr int kMaxIdLength = 16;

// Keys cv::waitKey reports that mean something other than "a character".
constexpr int kKeyEnter = 13;
constexpr int kKeyEscape = 27;
constexpr int kKeyBackspace = 8;

struct Button {
    cv::Rect rect;
    std::string label;
};

// Where the last click landed, filled by the mouse callback.
struct Click {
    int x = -1;
    int y = -1;
    bool happened = false;
};

class OpenCvRoomPrompt : public IRoomPrompt {
public:
    RoomChoice ask_room() override {
        const std::string window = "Room";
        cv::namedWindow(window, cv::WINDOW_AUTOSIZE);

        Click click;
        cv::setMouseCallback(
            window,
            [](int event, int x, int y, int, void* userdata) {
                if (event == cv::EVENT_LBUTTONDOWN) {
                    auto* c = static_cast<Click*>(userdata);
                    c->x = x;
                    c->y = y;
                    c->happened = true;
                }
            },
            &click);

        const Button create{{20, 150, 120, 45}, "Create"};
        const Button join{{170, 150, 120, 45}, "Join"};
        const Button cancel{{320, 150, 120, 45}, "Cancel"};

        RoomChoice choice;
        std::string typed;

        while (true) {
            cv::Mat frame(kHeight, kWidth, CV_8UC3, cv::Scalar(240, 240, 240));
            cv::putText(frame, "Room id (to Join). Create makes a new one:", {20, 35}, cv::FONT_HERSHEY_SIMPLEX, 0.5,
                        cv::Scalar(20, 20, 20), 1, cv::LINE_AA);

            // The text box, with a blinking caret so it reads as editable.
            cv::rectangle(frame, cv::Rect(20, 55, 420, 40), cv::Scalar(255, 255, 255), cv::FILLED);
            cv::rectangle(frame, cv::Rect(20, 55, 420, 40), cv::Scalar(120, 120, 120), 1);
            bool caret_on = (cv::getTickCount() / static_cast<int64>(cv::getTickFrequency() / 2)) % 2 == 0;
            cv::putText(frame, typed + (caret_on ? "_" : ""), {30, 83}, cv::FONT_HERSHEY_SIMPLEX, 0.7,
                        cv::Scalar(20, 20, 20), 2, cv::LINE_AA);

            for (const Button* b : {&create, &join, &cancel}) {
                cv::rectangle(frame, b->rect, cv::Scalar(210, 210, 210), cv::FILLED);
                cv::rectangle(frame, b->rect, cv::Scalar(90, 90, 90), 1);
                int baseline = 0;
                cv::Size ts = cv::getTextSize(b->label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);
                cv::putText(frame, b->label,
                            {b->rect.x + (b->rect.width - ts.width) / 2,
                             b->rect.y + (b->rect.height + ts.height) / 2},
                            cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(20, 20, 20), 1, cv::LINE_AA);
            }

            cv::imshow(window, frame);
            int key = cv::waitKey(30);

            // Enter is Join (the only action the typed id is for); Esc cancels.
            if (key == kKeyEnter) {
                choice.action = RoomChoice::Action::Join;
                choice.room_id = typed;
                break;
            }
            if (key == kKeyEscape) {
                choice.action = RoomChoice::Action::Cancel;
                break;
            }
            if (key == kKeyBackspace && !typed.empty()) {
                typed.pop_back();
            } else if (key > 32 && key < 127 && static_cast<int>(typed.size()) < kMaxIdLength) {
                // Room ids are generated uppercase, so accept either case and
                // normalise -- nobody should fail to join over a capital.
                typed.push_back(static_cast<char>(std::toupper(key)));
            }

            // The user closed the dialog with its own X button.
            if (cv::getWindowProperty(window, cv::WND_PROP_VISIBLE) < 1.0) {
                choice.action = RoomChoice::Action::Cancel;
                break;
            }

            if (click.happened) {
                click.happened = false;
                cv::Point at(click.x, click.y);
                if (create.rect.contains(at)) {
                    choice.action = RoomChoice::Action::Create;
                    break;
                }
                if (join.rect.contains(at)) {
                    choice.action = RoomChoice::Action::Join;
                    choice.room_id = typed;
                    break;
                }
                if (cancel.rect.contains(at)) {
                    choice.action = RoomChoice::Action::Cancel;
                    break;
                }
            }
        }

        cv::destroyWindow(window);
        // Pump the event loop so the window is really gone before the caller
        // draws anything else -- HighGUI tears down lazily otherwise.
        cv::waitKey(1);
        return choice;
    }

    void show_message(const std::string& title, const std::string& text) override {
        const std::string window = title;
        cv::namedWindow(window, cv::WINDOW_AUTOSIZE);

        // Wrapped by hand: OpenCV's putText draws a single line and knows
        // nothing about wrapping, and these messages are full sentences.
        std::vector<std::string> lines = wrap(text, 52);
        int height = 90 + static_cast<int>(lines.size()) * 28;

        while (true) {
            cv::Mat frame(height, kWidth, CV_8UC3, cv::Scalar(240, 240, 240));
            int y = 45;
            for (const std::string& line : lines) {
                cv::putText(frame, line, {20, y}, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(20, 20, 20), 1,
                            cv::LINE_AA);
                y += 28;
            }
            cv::putText(frame, "[ press any key ]", {20, height - 20}, cv::FONT_HERSHEY_SIMPLEX, 0.45,
                        cv::Scalar(90, 90, 90), 1, cv::LINE_AA);

            cv::imshow(window, frame);
            if (cv::waitKey(30) >= 0) {
                break;
            }
            if (cv::getWindowProperty(window, cv::WND_PROP_VISIBLE) < 1.0) {
                break;
            }
        }

        cv::destroyWindow(window);
        cv::waitKey(1);
    }

private:
    static std::vector<std::string> wrap(const std::string& text, std::size_t width) {
        std::vector<std::string> lines;
        std::string line;
        std::string word;
        auto flush_word = [&] {
            if (word.empty()) {
                return;
            }
            if (line.empty()) {
                line = word;
            } else if (line.size() + 1 + word.size() <= width) {
                line += " " + word;
            } else {
                lines.push_back(line);
                line = word;
            }
            word.clear();
        };
        for (char c : text) {
            if (c == ' ') {
                flush_word();
            } else {
                word.push_back(c);
            }
        }
        flush_word();
        if (!line.empty()) {
            lines.push_back(line);
        }
        return lines;
    }
};

}  // namespace

std::unique_ptr<IRoomPrompt> make_room_prompt() {
    return std::make_unique<OpenCvRoomPrompt>();
}

}  // namespace kfc::graphics::dialogs
