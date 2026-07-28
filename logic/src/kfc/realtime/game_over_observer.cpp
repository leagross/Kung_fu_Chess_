#include "../../../include/kfc/realtime/game_over_observer.hpp"

namespace kfc::model {

void GameOverObserver::on_arrival(const ArrivalEvent& event) {
    if (draw_ || !captured_a_king(event)) {
        return;
    }
    if (!winner_.has_value()) {
        winner_ = event.moved_piece.color;
        winner_decided_at_ms_ = event.arrived_at_ms;
    } else if (*winner_ != event.moved_piece.color && event.arrived_at_ms == winner_decided_at_ms_) {
        winner_.reset();
        draw_ = true;
    }
}

bool GameOverObserver::is_game_over() const {
    return draw_ || winner_.has_value();
}

std::optional<PieceColor> GameOverObserver::winner() const {
    return winner_;
}

bool GameOverObserver::is_draw() const {
    return draw_;
}

}  // namespace kfc::model
