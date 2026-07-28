#include "kfc/server/disconnect_watch.hpp"

namespace kfc::server {

DisconnectWatch::DisconnectWatch(int grace_ms) : grace_ms_(grace_ms) {}

void DisconnectWatch::report_disconnect(kfc::model::PieceColor color) {
    std::lock_guard<std::mutex> guard(mutex_);
    pending_ = color;
}

DisconnectWatch::Tick DisconnectWatch::advance(std::chrono::steady_clock::time_point now) {
    std::lock_guard<std::mutex> guard(mutex_);

    // Turn a freshly reported drop into a live countdown. Only one at a time --
    // a second drop while one is running is ignored.
    if (pending_.has_value() && !watching_.has_value()) {
        watching_ = pending_;
        deadline_ = now + std::chrono::milliseconds(grace_ms_);
        last_reported_second_ = -1;
    }
    pending_.reset();

    if (!watching_.has_value()) {
        return {};
    }

    int remaining_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(deadline_ - now).count());
    if (remaining_ms <= 0) {
        Tick tick;
        tick.expired_for = watching_;
        watching_.reset();  // stop watching before handing the expiry over
        return tick;
    }

    // Ceil, so the display runs N..1 and then the match ends -- never a
    // lingering "0" on screen.
    int seconds_remaining = (remaining_ms + 999) / 1000;
    if (seconds_remaining == last_reported_second_) {
        return {};  // same whole second as last tick; nothing new to say
    }
    last_reported_second_ = seconds_remaining;

    Tick tick;
    tick.seconds_remaining = seconds_remaining;
    return tick;
}

std::optional<kfc::model::PieceColor> DisconnectWatch::watching() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return watching_;
}

bool DisconnectWatch::cancel(kfc::model::PieceColor color) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!watching_.has_value() || *watching_ != color) {
        return false;  // already expired, or never this colour's countdown
    }
    watching_.reset();
    pending_.reset();
    return true;
}

void DisconnectWatch::clear() {
    std::lock_guard<std::mutex> guard(mutex_);
    watching_.reset();
    pending_.reset();
}

}  // namespace kfc::server
