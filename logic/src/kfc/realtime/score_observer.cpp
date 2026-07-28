#include "../../../include/kfc/realtime/score_observer.hpp"

namespace kfc::model {

ScoreObserver::ScoreObserver(const IPieceValueProvider& values) : values_(values) {}

void ScoreObserver::on_arrival(const ArrivalEvent& event) {
    if (!event.captured_piece.has_value()) {
        return;
    }

    int value = values_.value_of(event.captured_piece->kind);
    int& score = (event.moved_piece.color == PieceColor::White) ? white_score_ : black_score_;
    score += value;
}

int ScoreObserver::score(PieceColor color) const {
    return (color == PieceColor::White) ? white_score_ : black_score_;
}

}  // namespace kfc::model
