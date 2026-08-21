#pragma once

#include <optional>

#include "../../kfc/events/event_bus.hpp"
#include "../../kfc/input/controller.hpp"
#include "../../kfc/model/board.hpp"
#include "../../kfc/realtime/motion.hpp"

namespace kfc::texttests {

/// What a GUI needs from "the current game". Game implements this for local
/// play; a networked client (ServerLink) implements it backed by a server
/// connection instead -- callers never need to know which one they were handed.
class IGameView {
public:
    virtual ~IGameView() = default;

    virtual kfc::input::ControllerResult click(int x, int y) = 0;
    virtual kfc::input::ControllerResult jump(int x, int y) = 0;

    /// For Game, advances local simulated time. A networked implementation
    /// may treat this as a no-op -- its board only ever changes when the
    /// server says so, asynchronously, not on any local clock.
    virtual void wait(int ms) = 0;

    /// Publishes ArrivalEvent per resolved move, plus GameStarted/GameEnded.
    /// Wire subscriptions before the render loop starts: the bus is not
    /// synchronized.
    virtual kfc::events::EventBus& events() = 0;

    virtual const kfc::model::Board& board() const = 0;
    virtual std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const = 0;
    virtual bool is_piece_busy(kfc::model::PieceId piece_id) const = 0;
};

}  // namespace kfc::texttests
