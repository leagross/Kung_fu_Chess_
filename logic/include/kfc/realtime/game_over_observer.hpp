#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/realtime/game_observer.hpp"

namespace kfc::model {

/// Watches for a king capture and remembers who did it, for display only --
/// never gates or rejects a move (GameEngine::is_game_over() does that
/// independently). A second king capture only counts as a draw if its
/// arrived_at_ms exactly matches the first's; a later timestamp means the
/// first capture already decided the game.
class GameOverObserver : public IGameObserver {
public:
    void on_arrival(const ArrivalEvent& event) override;

    [[nodiscard]] bool is_game_over() const;

    /// The color that captured the opposing king. std::nullopt until
    /// is_game_over() is true, and also std::nullopt if is_draw() is true.
    [[nodiscard]] std::optional<PieceColor> winner() const;

    /// True if both kings were captured at the exact same simulated instant.
    [[nodiscard]] bool is_draw() const;

private:
    std::optional<PieceColor> winner_;
    long long winner_decided_at_ms_ = 0;
    bool draw_ = false;
};

}  // namespace kfc::model
