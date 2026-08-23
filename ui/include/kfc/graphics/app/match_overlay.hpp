#pragma once

#include <chrono>
#include <optional>

#include "kfc/events/event_bus.hpp"
#include "kfc/events/game_events.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics::app {

inline constexpr int kDefaultIntroDurationMs = 1500;

enum class Overlay {
    None,
    Searching,  // networked, seated, waiting for a rating-compatible opponent
    Intro,      // first moment of a match: the title, fading out
    Countdown,  // a dropped opponent's grace period
    GameOver,
};

/// Priority: Searching > GameOver > Countdown > Intro (Countdown/Intro can overlap).
class MatchOverlay {
public:
    using Clock = std::chrono::steady_clock;

    /// bus must outlive this object.
    explicit MatchOverlay(kfc::events::EventBus& bus, int intro_duration_ms = kDefaultIntroDurationMs);

    /// searching is the caller's own signal, since nothing publishes it on the bus.
    [[nodiscard]] Overlay current(bool searching, Clock::time_point now) const;

    /// 1.0 at match start, fading to 0.0. Only meaningful while current() is Intro.
    [[nodiscard]] double intro_opacity(Clock::time_point now) const;

    /// std::nullopt for a draw. Only meaningful while current() is GameOver.
    [[nodiscard]] std::optional<kfc::model::PieceColor> winner() const { return winner_; }

    /// Only meaningful while current() is Countdown.
    [[nodiscard]] int countdown_seconds() const { return countdown_seconds_.value_or(0); }

private:
    int intro_duration_ms_;

    bool started_ = false;
    bool ended_ = false;
    std::optional<kfc::model::PieceColor> winner_;
    std::optional<int> countdown_seconds_;
    Clock::time_point started_at_{};
};

}  // namespace kfc::graphics::app
