#pragma once

#include <memory>
#include <string>

namespace kfc::graphics::dialogs {

/// room_id is only meaningful for Join -- Create asks the server to generate one.
struct RoomChoice {
    enum class Action { Create, Join, Cancel };
    Action action = Action::Cancel;
    std::string room_id;
};

/// Meaningful only when !cancelled.
struct LoginChoice {
    std::string username;
    std::string password;
    bool cancelled = false;
};

/// Platform-specific native GUI (login dialog, room dialog, message box); see make_room_prompt().
class IRoomPrompt {
public:
    virtual ~IRoomPrompt() = default;

    /// No separate Register screen -- an unseen username registers on first login.
    [[nodiscard]] virtual LoginChoice ask_login() = 0;

    [[nodiscard]] virtual RoomChoice ask_room() = 0;

    virtual void show_message(const std::string& title, const std::string& text) = 0;
};

[[nodiscard]] std::unique_ptr<IRoomPrompt> make_room_prompt();

}  // namespace kfc::graphics::dialogs
