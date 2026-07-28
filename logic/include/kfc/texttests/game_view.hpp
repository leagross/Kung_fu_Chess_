#pragma once

#include <optional>

#include "../../kfc/events/event_bus.hpp"
#include "../../kfc/input/controller.hpp"
#include "../../kfc/model/board.hpp"
#include "../../kfc/realtime/motion.hpp"

namespace kfc::texttests {

/// What a GUI actually needs from "the current game" -- exactly the subset
/// of Game's public surface MouseInputAdapter and PieceAnimatorRegistry
/// call. Game implements this for local single-player play; a networked
/// client (ServerLink, in ui/graphics/net) implements it too, backed
/// by a server connection instead of a local GameEngine -- neither
/// MouseInputAdapter nor PieceAnimatorRegistry ever needs to know which one
/// they were handed.
class IGameView {
public:
    virtual ~IGameView() = default;

    virtual kfc::input::ControllerResult click(int x, int y) = 0;
    virtual kfc::input::ControllerResult jump(int x, int y) = 0;

    /// For Game, advances local simulated time. A networked implementation
    /// may treat this as a no-op -- its board only ever changes when the
    /// server says so, asynchronously, not on any local clock.
    virtual void wait(int ms) = 0;

    /// The event bus this view publishes game events on -- ArrivalEvent per
    /// resolved move, plus GameStarted/GameEnded (see kfc/events). A UI
    /// subscribes its score panel, move log, sound, and start/end animations
    /// here; the view never needs to know who, if anyone, is listening. Wire
    /// all subscriptions before the render loop starts: neither Game nor
    /// ServerLink synchronizes the bus, and both publish from the same single
    /// thread that then drives rendering.
    virtual kfc::events::EventBus& events() = 0;

    virtual const kfc::model::Board& board() const = 0;
    virtual std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const = 0;
    virtual bool is_piece_busy(kfc::model::PieceId piece_id) const = 0;
};

}  // namespace kfc::texttests
