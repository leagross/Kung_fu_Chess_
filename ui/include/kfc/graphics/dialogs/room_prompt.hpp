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

/// What the player typed into the Login dialog. Only meaningful when
/// !cancelled -- see ask_login's own doc comment for what an empty username
/// or password means once that holds.
struct LoginChoice {
    std::string username;
    std::string password;
    bool cancelled = false;
};

/// The small amount of native GUI this game needs beyond its OpenCV window: a
/// login dialog (username + masked password + Login / Cancel), a room dialog
/// (text box + Create / Join / Cancel), and a plain message box.
///
/// An interface, rather than direct calls, for one reason: these are the only
/// things in the whole project that cannot be written once and compiled
/// everywhere. Behind it, each platform has its own implementation and the
/// single #ifdef in the codebase lives in make_room_prompt(). Everything above
/// this line -- the render loop, the home screen, the whole client -- is
/// ordinary portable C++.
///
/// (The same shape as kfc::audio::ISoundPlayer, for the same reason.)
class IRoomPrompt {
public:
    virtual ~IRoomPrompt() = default;

    /// Shows the Login dialog and blocks until the player presses a button.
    /// There is no separate Register screen: a username never seen before
    /// registers on the first successful login with whatever password was
    /// typed (see kfc::database::UserRepository::authenticate) -- the same
    /// account-creation rule this dialog replaces the old --username flag
    /// and terminal password prompt for, just made visible instead of
    /// implicit. cancelled is true only if the player pressed Cancel or
    /// closed the dialog; username/password are both meaningless then.
    [[nodiscard]] virtual LoginChoice ask_login() = 0;

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
