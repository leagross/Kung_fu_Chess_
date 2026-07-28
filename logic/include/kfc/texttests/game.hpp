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

/// The local-play host for one playable game: owns a GameCore (the shared
/// Board -> RuleEngine -> RealTimeArbiter -> MotionFactory -> GameEngine
/// chain, assembled identically by the server's Match) plus the two things
/// that are specific to local single-player play -- the Controller that sits
/// in front of GameEngine for click routing, and the event bus every arrival
/// is published on. CommandProcessor drives a Game through the four-command
/// DSL; nothing here parses text or touches stdin/stdout. Implements
/// IGameView so kfc_gui_app's local single-player path and its networked
/// path (ServerLink) are interchangeable to MouseInputAdapter/
/// PieceAnimatorRegistry.
class Game : public IGameView {
public:
    /// Builds a GameCore around the given board (which registers every known
    /// piece kind's movement rule and wires the full simulation chain), then
    /// puts a Controller in front of it. speed_provider/meters_per_cell/
    /// standard_policy/jump_policy are forwarded straight through to the
    /// GameCore's MotionFactory unchanged -- see its own constructor for what
    /// they mean and why they default the way they do. standard_policy/
    /// jump_policy default to the fixed backend constants; a caller that
    /// wants cooldowns tied to its own animation timing (e.g. a GUI app
    /// reading rest-clip durations out of config.json) passes its own
    /// ICooldownPolicy implementations instead.
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
    // core_ must precede controller_: the Controller is constructed with
    // references into core_ (its board and its GameEngine as IMoveRequester).
    kfc::model::GameCore core_;
    kfc::input::Controller controller_;
    kfc::events::EventBus events_;

    // Lifecycle-event bookkeeping for the bus: GameStarted is published on the
    // first wait(), GameEnded once game_over_ sees a king captured. game_over_
    // is Game's own detector, independent of any GameOverObserver the UI runs
    // separately for its banner -- Game owns the authoritative "the game just
    // ended" signal so every listener (sound, end animation) hears it the same
    // way, local or networked.
    kfc::model::GameOverObserver game_over_;
    bool started_ = false;
    bool ended_ = false;
};

}  // namespace kfc::texttests
