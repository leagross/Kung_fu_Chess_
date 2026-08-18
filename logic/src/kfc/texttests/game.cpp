#include "../../../include/kfc/texttests/game.hpp"

#include "../../../include/kfc/events/game_events.hpp"
#include "../../../include/kfc/io/board_printer.hpp"

using kfc::model::Board;

namespace kfc::texttests {

Game::Game(Board board, const kfc::model::IPieceSpeedProvider& speed_provider, double meters_per_cell,
           const kfc::model::ICooldownPolicy& standard_policy, const kfc::model::ICooldownPolicy& jump_policy)
    : core_(std::move(board), standard_policy, jump_policy, speed_provider, meters_per_cell),
      controller_(core_.board(), core_.engine(),
                  kfc::input::BoardMapper(core_.board().width(), core_.board().height())) {}

kfc::input::ControllerResult Game::click(int x, int y) {
    return controller_.click(x, y);
}

kfc::input::ControllerResult Game::jump(int x, int y) {
    return controller_.jump(x, y);
}

void Game::wait(int ms) {
    // Published here rather than the constructor so subscribers wired up
    // after construction don't miss it.
    if (!started_) {
        started_ = true;
        events_.publish(kfc::events::GameStarted{});
    }

    kfc::model::ArrivalEvents events = core_.engine().wait(ms);
    for (const kfc::model::ArrivalEvent& event : events) {
        events_.publish(event);
        game_over_.on_arrival(event);
    }

    if (!ended_ && game_over_.is_game_over()) {
        ended_ = true;
        events_.publish(kfc::events::GameEnded{game_over_.is_draw() ? std::nullopt : game_over_.winner()});
    }
}

kfc::events::EventBus& Game::events() {
    return events_;
}

std::string Game::print_board() const {
    return kfc::io::BoardPrinter{}.print(core_.board());
}

const Board& Game::board() const {
    return core_.board();
}

std::optional<kfc::model::Motion> Game::motion_for(kfc::model::PieceId piece_id) const {
    return core_.arbiter().motion_for(piece_id);
}

bool Game::is_piece_busy(kfc::model::PieceId piece_id) const {
    return core_.arbiter().is_piece_busy(piece_id);
}

}  // namespace kfc::texttests
