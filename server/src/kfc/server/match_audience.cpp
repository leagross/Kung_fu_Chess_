#include "kfc/server/match_audience.hpp"

#include <utility>

namespace kfc::server {

std::optional<kfc::model::PieceColor> MatchAudience::seat(const std::string& username, SendFn send, CloseFn close) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (!white_send_.has_value()) {
        white_send_ = std::move(send);
        white_close_ = std::move(close);
        white_username_ = username;
        return kfc::model::PieceColor::White;
    }
    if (!black_send_.has_value()) {
        black_send_ = std::move(send);
        black_close_ = std::move(close);
        black_username_ = username;
        return kfc::model::PieceColor::Black;
    }
    return std::nullopt;  // both seats taken
}

void MatchAudience::watch(SendFn send, CloseFn close) {
    std::lock_guard<std::mutex> guard(mutex_);
    spectator_sends_.push_back(std::move(send));
    if (close) {
        spectator_closes_.push_back(std::move(close));
    }
}

void MatchAudience::reseat(kfc::model::PieceColor color, SendFn send, CloseFn close) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (color == kfc::model::PieceColor::White) {
        white_send_ = std::move(send);
        white_close_ = std::move(close);
    } else {
        black_send_ = std::move(send);
        black_close_ = std::move(close);
    }
    // Username deliberately untouched: reseating is the same player returning.
}

bool MatchAudience::both_seats_taken() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return white_send_.has_value() && black_send_.has_value();
}

std::string MatchAudience::username_of(kfc::model::PieceColor color) const {
    std::lock_guard<std::mutex> guard(mutex_);
    return color == kfc::model::PieceColor::White ? white_username_ : black_username_;
}

void MatchAudience::broadcast(const std::string& encoded) const {
    std::lock_guard<std::mutex> guard(mutex_);
    if (white_send_.has_value()) {
        (*white_send_)(encoded);
    }
    if (black_send_.has_value()) {
        (*black_send_)(encoded);
    }
    for (const SendFn& watcher : spectator_sends_) {
        watcher(encoded);
    }
}

void MatchAudience::send_to(kfc::model::PieceColor color, const std::string& encoded) const {
    std::lock_guard<std::mutex> guard(mutex_);
    const std::optional<SendFn>& target = color == kfc::model::PieceColor::White ? white_send_ : black_send_;
    if (target.has_value()) {
        (*target)(encoded);
    }
}

void MatchAudience::release_all() const {
    // Copied out, then called with the lock released: closing a socket is
    // network I/O, and the close comes back to the Match as an ordinary
    // disconnect -- which would re-enter this table and deadlock on mutex_ if
    // it were still held.
    std::optional<CloseFn> white;
    std::optional<CloseFn> black;
    std::vector<CloseFn> watchers;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        white = white_close_;
        black = black_close_;
        watchers = spectator_closes_;
    }

    if (white.has_value()) {
        (*white)();
    }
    if (black.has_value()) {
        (*black)();
    }
    for (const CloseFn& watcher : watchers) {
        watcher();
    }
}

}  // namespace kfc::server
