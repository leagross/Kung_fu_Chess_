#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/realtime/game_observer.hpp"

namespace kfc::model {

/// Watches for a king capture and remembers who did it, for display only.
class GameOverObserver : public IGameObserver {
public:
    void on_arrival(const ArrivalEvent& event) override;

    [[nodiscard]] bool is_game_over() const;

    /// nullopt until is_game_over(), and also nullopt if is_draw().
    [[nodiscard]] std::optional<PieceColor> winner() const;

    /// True if both kings were captured at the exact same simulated instant.
    [[nodiscard]] bool is_draw() const;

private:
    std::optional<PieceColor> winner_;
    long long winner_decided_at_ms_ = 0;
    bool draw_ = false;
};

}  // namespace kfc::model
