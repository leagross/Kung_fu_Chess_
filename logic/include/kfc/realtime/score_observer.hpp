#pragma once

#include "../../kfc/model/piece.hpp"
#include "../../kfc/realtime/game_observer.hpp"
#include "../../kfc/realtime/piece_value_provider.hpp"

namespace kfc::model {

/// Tallies material score per side. Credits whichever side made the capturing
/// move, not the captured side. The per-kind point values come from an
/// injected IPieceValueProvider (defaulting to the standard chess values), so
/// scoring can be re-tuned as gameplay data without touching this class.
class ScoreObserver : public IGameObserver {
public:
    /// values must outlive this ScoreObserver. Defaults to the standard
    /// chess material values.
    explicit ScoreObserver(const IPieceValueProvider& values = kDefaultPieceValueProvider);

    void on_arrival(const ArrivalEvent& event) override;

    [[nodiscard]] int score(PieceColor color) const;

private:
    const IPieceValueProvider& values_;
    int white_score_ = 0;
    int black_score_ = 0;
};

}  // namespace kfc::model
