#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/realtime/game_observer.hpp"

namespace kfc::model {

/// Watches for a king capture and remembers who did it -- the same
/// condition GameEngine::is_game_over() checks internally to reject further
/// moves, but tracked independently here for display purposes only. This
/// class never gates or rejects anything; it only ever answers "is it over,
/// and who won" for whoever draws the result.
///
/// If the opposing king is captured while a winner is already recorded, it's
/// a draw only when that second capture's ArrivalEvent::arrived_at_ms
/// exactly matches the first's -- i.e. both kings genuinely died at the same
/// simulated instant, not merely within the same advance_time call (a
/// single coarse call can still report two arrivals at different times).
/// A later, different timestamp means the game was already decided by the
/// first capture, so the second is not a draw -- it should not even be
/// possible once nothing keeps generating arrivals after game-over, but this
/// observer does not depend on that; it discards a late second capture on
/// its own.
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
