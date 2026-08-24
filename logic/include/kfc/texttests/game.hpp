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

/// Local-play host for one playable game; implements IGameView so local and networked play are interchangeable.
class Game : public IGameView {
public:
    /// speed_provider/meters_per_cell/standard_policy/jump_policy forward to GameCore's MotionFactory unchanged.
    explicit Game(kfc::model::Board board,
                  const kfc::model::IPieceSpeedProvider& speed_provider = kfc::model::kDefaultPieceSpeedProvider,
                  double meters_per_cell = kfc::model::kDefaultMetersPerCell,
                  const kfc::model::ICooldownPolicy& standard_policy = kfc::model::kDefaultStandardCooldownPolicy,
                  const kfc::model::ICooldownPolicy& jump_policy = kfc::model::kDefaultJumpCooldownPolicy);

    kfc::input::ControllerResult click(int x, int y) override;

    /// Independent of any click selection in progress.
    kfc::input::ControllerResult jump(int x, int y) override;

    void wait(int ms) override;

    kfc::events::EventBus& events() override;

    std::string print_board() const;

    const kfc::model::Board& board() const override;

    std::optional<kfc::model::Motion> motion_for(kfc::model::PieceId piece_id) const override;

    bool is_piece_busy(kfc::model::PieceId piece_id) const override;

private:
    kfc::model::GameCore core_;  // must precede controller_: Controller holds references into core_
    kfc::input::Controller controller_;
    kfc::events::EventBus events_;

    kfc::model::GameOverObserver game_over_;
    bool started_ = false;
    bool ended_ = false;
};

}  // namespace kfc::texttests
