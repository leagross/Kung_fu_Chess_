#include "kfc/graphics/app/match_overlay.hpp"

#include <algorithm>
#include <chrono>

namespace kfc::graphics::app {

MatchOverlay::MatchOverlay(kfc::events::EventBus& bus, int intro_duration_ms)
    : bus_(bus), intro_duration_ms_(intro_duration_ms) {
    on_started_ = bus_.subscribe<kfc::events::GameStarted>([this](const kfc::events::GameStarted&) {
        started_ = true;
        started_at_ = Clock::now();
    });

    on_ended_ = bus_.subscribe<kfc::events::GameEnded>([this](const kfc::events::GameEnded& event) {
        ended_ = true;
        winner_ = event.winner;
        countdown_seconds_.reset();
    });

    on_countdown_ = bus_.subscribe<kfc::events::OpponentCountdown>(
        [this](const kfc::events::OpponentCountdown& event) { countdown_seconds_ = event.seconds_remaining; });

    // The other way a countdown ends: they returned in time.
    on_opponent_returned_ = bus_.subscribe<kfc::events::OpponentReturned>(
        [this](const kfc::events::OpponentReturned&) { countdown_seconds_.reset(); });
}

MatchOverlay::~MatchOverlay() {
    bus_.unsubscribe(on_started_);
    bus_.unsubscribe(on_ended_);
    bus_.unsubscribe(on_countdown_);
    bus_.unsubscribe(on_opponent_returned_);
}

Overlay MatchOverlay::current(bool searching, Clock::time_point now) const {
    // Order is priority; see the header for why each outranks the next.
    if (searching) {
        return Overlay::Searching;
    }
    if (ended_) {
        return Overlay::GameOver;
    }
    if (countdown_seconds_.has_value()) {
        return Overlay::Countdown;
    }
    if (started_ && now - started_at_ < std::chrono::milliseconds(intro_duration_ms_)) {
        return Overlay::Intro;
    }
    return Overlay::None;
}

double MatchOverlay::intro_opacity(Clock::time_point now) const {
    if (!started_ || intro_duration_ms_ <= 0) {
        return 0.0;
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - started_at_).count();
    double remaining = 1.0 - static_cast<double>(elapsed) / intro_duration_ms_;
    return std::clamp(remaining, 0.0, 1.0);
}

}  // namespace kfc::graphics::app
