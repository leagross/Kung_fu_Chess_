#pragma once

#include <memory>
#include <string>

namespace kfc::graphics::dialogs {

/// What the player chose in the Room dialog. room_id is only meaningful for
/// Join -- Create asks the server to generate one (see kfc::protocol::CreateRoom),
/// so nothing typed is used for it.
struct RoomChoice {
    enum class Action { Create, Join, Cancel };
    Action action = Action::Cancel;
    std::string room_id;
};

/// The small amount of native GUI this game needs beyond its OpenCV window: a
/// room dialog (text box + Create / Join / Cancel) and a plain message box.
///
/// An interface, rather than direct calls, for one reason: these are the only
/// two things in the whole project that cannot be written once and compiled
/// everywhere. Behind it, each platform has its own implementation and the
/// single #ifdef in the codebase lives in make_room_prompt(). Everything above
/// this line -- the render loop, the home screen, the whole client -- is
/// ordinary portable C++.
///
/// (The same shape as kfc::audio::ISoundPlayer, for the same reason.)
class IRoomPrompt {
public:
    virtual ~IRoomPrompt() = default;

    /// Shows the Room dialog and blocks until the player presses a button.
    [[nodiscard]] virtual RoomChoice ask_room() = 0;

    /// Shows a message and blocks until the player dismisses it. Used for
    /// "no opponent found" and for a refused join.
    virtual void show_message(const std::string& title, const std::string& text) = 0;
};

/// The implementation for the platform this was built for. The one place in the
/// project that asks which OS it is running on.
[[nodiscard]] std::unique_ptr<IRoomPrompt> make_room_prompt();

}  // namespace kfc::graphics::dialogs
