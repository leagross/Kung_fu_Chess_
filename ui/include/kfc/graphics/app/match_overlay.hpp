#pragma once

#include <chrono>
#include <optional>

#include "kfc/events/event_bus.hpp"
#include "kfc/events/game_events.hpp"
#include "kfc/model/piece.hpp"

namespace kfc::graphics::app {

/// How long the "KUNG FU CHESS" intro splash takes to fade out.
inline constexpr int kDefaultIntroDurationMs = 1500;

/// What the board should be wearing right now, if anything.
enum class Overlay {
    None,
    /// Networked, seated, still waiting for a rating-compatible opponent.
    Searching,
    /// The first moment of a match: the title, fading out.
    Intro,
    /// A dropped opponent's grace period, counting down.
    Countdown,
    GameOver,
};

/// Follows whole-game signals on the event bus and answers which banner goes
/// on the board this frame. Priority: Searching outranks everything (no game
/// yet); GameOver supersedes Countdown; Countdown outranks Intro (they can
/// overlap) and also ends when the opponent returns, not just on game end.
///
/// Free of OpenCV and drawing -- says only *which* overlay, so it's testable
/// with a bare EventBus. Subscribes on construction; read on the render
/// thread, same as any other bus subscriber.
class MatchOverlay {
public:
    using Clock = std::chrono::steady_clock;

    /// bus must outlive this object. intro_duration_ms is how long the intro
    /// splash lingers.
    explicit MatchOverlay(kfc::events::EventBus& bus, int intro_duration_ms = kDefaultIntroDurationMs);
    ~MatchOverlay();

    MatchOverlay(const MatchOverlay&) = delete;
    MatchOverlay& operator=(const MatchOverlay&) = delete;

    /// searching is the caller's own signal (not on the bus, since nothing
    /// publishes it). now is passed in so the intro is testable without a wait.
    [[nodiscard]] Overlay current(bool searching, Clock::time_point now) const;

    /// 1.0 at match start, fading to 0.0. Only meaningful while current() is Intro.
    [[nodiscard]] double intro_opacity(Clock::time_point now) const;

    /// Winner of a decided match; std::nullopt for a draw. Only meaningful
    /// while current() is GameOver.
    [[nodiscard]] std::optional<kfc::model::PieceColor> winner() const { return winner_; }

    /// Only meaningful while current() is Countdown.
    [[nodiscard]] int countdown_seconds() const { return countdown_seconds_.value_or(0); }

private:
    kfc::events::EventBus& bus_;
    int intro_duration_ms_;

    bool started_ = false;
    bool ended_ = false;
    std::optional<kfc::model::PieceColor> winner_;
    std::optional<int> countdown_seconds_;
    Clock::time_point started_at_{};

    kfc::events::EventBus::SubscriptionId on_started_;
    kfc::events::EventBus::SubscriptionId on_ended_;
    kfc::events::EventBus::SubscriptionId on_countdown_;
    kfc::events::EventBus::SubscriptionId on_opponent_returned_;
};

}  // namespace kfc::graphics::app
