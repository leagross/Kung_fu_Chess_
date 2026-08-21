#pragma once

#include <optional>
#include <string>

#include "../../kfc/model/board.hpp"
#include "../../kfc/engine/game_core.hpp"
#include "../../kfc/events/event_bus.hpp"
#include "../../kfc/input/board_mapper.hpp"
#include "../../kfc/input/controller.hpp"
#include "../../kfc/realtime/cooldown_policy.hpp"
#include "../../kfc/realtime/game_over_observer.hpp"
#include "../../kfc/realtime/jump_cooldown_policy.hpp"
#include "../../kfc/realtime/motion_factory.hpp"
#include "../../kfc/realtime/standard_cooldown_policy.hpp"
#include "../../kfc/texttests/game_view.hpp"

namespace kfc::texttests {

/// The local-play host for one playable game: owns a GameCore plus a
/// Controller for click routing and the event bus arrivals publish on.
/// Implements IGameView so local and networked play are interchangeable.
class Game : public IGameView {
public:
    /// speed_provider/meters_per_cell/standard_policy/jump_policy are
    /// forwarded to GameCore's MotionFactory unchanged; policies default to
    /// the fixed backend constants unless the caller supplies its own.
    explicit Game(kfc::model::Board board,
                  const kfc::model::IPieceSpeedProvider& speed_provider = kfc::model::kDefaultPieceSpeedProvider,
                  double meters_per_cell = kfc::model::kDefaultMetersPerCell,
                  const kfc::model::ICooldownPolicy& standard_policy = kfc::model::kDefaultStandardCooldownPolicy,
                  const kfc::model::ICooldownPolicy& jump_policy = kfc::model::kDefaultJumpCooldownPolicy);

    /// Routes one click through Controller, which decides selection vs.
    /// move request on its own.
    kfc::input::ControllerResult click(int x, int y) override;

    /// Routes one jump-in-place request through Controller. Independent of
    /// any click selection in progress.
    kfc::input::ControllerResult jump(int x, int y) override;

    /// Advances simulated time by ms via GameEngine::wait, then publishes each
    /// resulting ArrivalEvent on the bus, in order -- after the fact, never
    /// affecting whether or how a move happened.
    void wait(int ms) override;

    /// The event bus arrivals are published on -- see IGameView::events.
    kfc::events::EventBus& events() override;

    /// The current logical board, rendered as text (BoardPrinter's format).
    std::string print_board() const;

    /// Read-only access to the live board, for callers that need to inspect
    /// piece placement directly (e.g. a renderer) instead of through
    /// BoardPrinter's text format.
    const kfc::model::Board& board() const override;

    /// The in-flight Motion for piece_id, if any -- see
    /// RealTimeArbiter::motion_for. For a renderer that needs to
    /// interpolate a moving piece's on-screen position, or tell a Move
    /// apart from a JumpInPlace.
    std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const override;

    /// True if piece_id has a motion in flight or is resting in cooldown --
    /// see RealTimeArbiter::is_piece_busy.
    bool is_piece_busy(kfc::model::PieceId piece_id) const override;

private:
    // core_ must precede controller_: Controller holds references into core_.
    kfc::model::GameCore core_;
    kfc::input::Controller controller_;
    kfc::events::EventBus events_;

    // GameStarted publishes on the first wait(); GameEnded once game_over_
    // sees a king captured.
    kfc::model::GameOverObserver game_over_;
    bool started_ = false;
    bool ended_ = false;
};

}  // namespace kfc::texttests
