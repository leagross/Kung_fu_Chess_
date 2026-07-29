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
    /// Nothing -- an ordinary frame of an ordinary game.
    None,
    /// Networked, seated, still waiting for a rating-compatible opponent.
    Searching,
    /// The first moment of a match: the title, fading out.
    Intro,
    /// A dropped opponent's grace period, counting down.
    Countdown,
    /// The match is decided.
    GameOver,
};

/// Follows the whole-game signals on the event bus and answers the one question
/// the render loop actually asks: which banner goes on the board this frame.
///
/// Pulled out of the GUI's main() because it is not graphics -- it is a small
/// state machine with rules that are easy to get subtly wrong and were, until
/// now, spread across four separate bus subscriptions and four loose local
/// variables read by an if/else chain a hundred lines further down. Its rules
/// are worth stating in one place, and worth testing:
///
///  - **Searching outranks everything.** There is no game yet to end or count
///    down; whatever else may have been published, an unseated client is
///    waiting.
///  - **The end banner supersedes a countdown.** The countdown means "they might
///    still come back"; once the game is decided that question is settled, so
///    GameEnded clears it rather than letting both try to draw.
///  - **A countdown outranks the intro.** They can genuinely overlap -- an
///    opponent who drops within the first second and a half of a match -- and
///    the countdown is the one the player needs.
///  - **A countdown also ends by the opponent returning**, not only by the game
///    ending. Forgetting that leaves a stale countdown frozen on screen for the
///    rest of a game that is being played normally.
///
/// Deliberately free of OpenCV, and of any drawing at all: it says *which*
/// overlay, never how to render one. That is what lets it be tested with a bare
/// EventBus and no window.
///
/// Threading: subscribes on construction and is read on the render thread, like
/// every other bus subscriber (see EventBus -- handlers run on whoever
/// publishes, which here is that same thread).
class MatchOverlay {
public:
    using Clock = std::chrono::steady_clock;

    /// Subscribes to the bus for the rest of this object's life. bus must
    /// outlive it. intro_duration_ms is how long the intro splash lingers.
    explicit MatchOverlay(kfc::events::EventBus& bus, int intro_duration_ms = kDefaultIntroDurationMs);

    /// Which banner to draw. searching is the caller's own answer to "networked
    /// and seated but no opponent yet" -- it is not on the bus, because nothing
    /// publishes it. now is passed in rather than read from a clock so the intro
    /// can be tested without waiting for it.
    [[nodiscard]] Overlay current(bool searching, Clock::time_point now) const;

    /// How solid the intro splash is: 1.0 the instant the match starts, fading
    /// to 0.0 as the intro elapses. Only meaningful while current() is Intro.
    [[nodiscard]] double intro_opacity(Clock::time_point now) const;

    /// The winner of a decided match; std::nullopt for a draw. Only meaningful
    /// while current() is GameOver.
    [[nodiscard]] std::optional<kfc::model::PieceColor> winner() const { return winner_; }

    /// Seconds left on a dropped opponent's grace. Only meaningful while
    /// current() is Countdown.
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
