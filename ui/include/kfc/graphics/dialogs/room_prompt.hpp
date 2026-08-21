#pragma once

#include <memory>
#include <string>

namespace kfc::graphics::dialogs {

/// What the player chose in the Room dialog. room_id is only meaningful for
/// Join -- Create asks the server to generate one.
struct RoomChoice {
    enum class Action { Create, Join, Cancel };
    Action action = Action::Cancel;
    std::string room_id;
};

/// What the player typed into the Login dialog. Meaningful only when
/// !cancelled.
struct LoginChoice {
    std::string username;
    std::string password;
    bool cancelled = false;
};

/// The native GUI this game needs beyond its OpenCV window: a login dialog,
/// a room dialog, and a message box. Each platform has its own
/// implementation behind this interface; the single #ifdef lives in
/// make_room_prompt() (same shape as kfc::audio::ISoundPlayer, same reason).
class IRoomPrompt {
public:
    virtual ~IRoomPrompt() = default;

    /// Shows the Login dialog and blocks until the player presses a button.
    /// No separate Register screen -- a username never seen before
    /// registers on first successful login (see UserRepository::authenticate).
    [[nodiscard]] virtual LoginChoice ask_login() = 0;

    /// Shows the Room dialog and blocks until the player presses a button.
    [[nodiscard]] virtual RoomChoice ask_room() = 0;

    /// Shows a message and blocks until the player dismisses it. Used for
    /// "no opponent found" and for a refused join.
    virtual void show_message(const std::string& title, const std::string& text) = 0;
};

/// The implementation for the platform this was built for.
[[nodiscard]] std::unique_ptr<IRoomPrompt> make_room_prompt();

}  // namespace kfc::graphics::dialogs
