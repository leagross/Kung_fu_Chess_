#include "kfc/server/match_audience.hpp"

#include <algorithm>
#include <utility>

namespace kfc::server {

std::shared_ptr<const MatchAudience::Roster> MatchAudience::current() const {
    std::lock_guard<std::mutex> guard(mutex_);
    return roster_;
}

std::shared_ptr<MatchAudience::Roster> MatchAudience::editable_copy() const {
    return std::make_shared<Roster>(*roster_);
}

std::optional<kfc::model::PieceColor> MatchAudience::seat(const std::string& username, SendFn send, CloseFn close) {
    std::lock_guard<std::mutex> guard(mutex_);
    std::shared_ptr<Roster> next = editable_copy();
    kfc::model::PieceColor assigned = kfc::model::PieceColor::White;

    if (!next->white_send.has_value()) {
        next->white_send = std::move(send);
        next->white_close = std::move(close);
        next->white_username = username;
        assigned = kfc::model::PieceColor::White;
    } else if (!next->black_send.has_value()) {
        next->black_send = std::move(send);
        next->black_close = std::move(close);
        next->black_username = username;
        assigned = kfc::model::PieceColor::Black;
    } else {
        return std::nullopt;  // both seats taken
    }

    roster_ = std::move(next);
    // Released after publishing, so seeing the count reach two also sees the seats.
    seats_filled_.fetch_add(1, std::memory_order_release);
    return assigned;
}

WatcherId MatchAudience::watch(SendFn send, CloseFn close) {
    std::lock_guard<std::mutex> guard(mutex_);
    if (roster_->watchers.size() >= kMaxSpectators) {
        return 0;  // "not a watcher" -- see WatcherId's own doc comment
    }
    std::shared_ptr<Roster> next = editable_copy();
    WatcherId id = next_watcher_id_++;
    next->watchers.push_back(Watcher{id, std::move(send), std::move(close)});
    roster_ = std::move(next);
    return id;
}

void MatchAudience::unwatch(WatcherId id) {
    std::lock_guard<std::mutex> guard(mutex_);
    // Checked before copying to avoid a wasted copy/publish on a double close.
    auto is_id = [id](const Watcher& watcher) { return watcher.id == id; };
    if (std::none_of(roster_->watchers.begin(), roster_->watchers.end(), is_id)) {
        return;
    }

    std::shared_ptr<Roster> next = editable_copy();
    next->watchers.erase(std::remove_if(next->watchers.begin(), next->watchers.end(), is_id),
                         next->watchers.end());
    roster_ = std::move(next);
}

void MatchAudience::reseat(kfc::model::PieceColor color, SendFn send, CloseFn close) {
    std::lock_guard<std::mutex> guard(mutex_);
    std::shared_ptr<Roster> next = editable_copy();
    if (color == kfc::model::PieceColor::White) {
        next->white_send = std::move(send);
        next->white_close = std::move(close);
    } else {
        next->black_send = std::move(send);
        next->black_close = std::move(close);
    }
    // Username deliberately untouched.
    roster_ = std::move(next);
}

std::string MatchAudience::username_of(kfc::model::PieceColor color) const {
    std::shared_ptr<const Roster> roster = current();
    return color == kfc::model::PieceColor::White ? roster->white_username : roster->black_username;
}

std::size_t MatchAudience::watcher_count() const {
    return current()->watchers.size();
}

void MatchAudience::broadcast(const std::string& encoded) const {
    std::shared_ptr<const Roster> roster = current();
    if (roster->white_send.has_value()) {
        (*roster->white_send)(encoded);
    }
    if (roster->black_send.has_value()) {
        (*roster->black_send)(encoded);
    }
    for (const Watcher& watcher : roster->watchers) {
        watcher.send(encoded);
    }
}

void MatchAudience::send_to(kfc::model::PieceColor color, const std::string& encoded) const {
    std::shared_ptr<const Roster> roster = current();
    const std::optional<SendFn>& target =
        color == kfc::model::PieceColor::White ? roster->white_send : roster->black_send;
    if (target.has_value()) {
        (*target)(encoded);
    }
}

void MatchAudience::release_all() const {
    // Closing a socket re-enters this table as an ordinary disconnect, which
    // would deadlock on mutex_ if it were still held here.
    std::shared_ptr<const Roster> roster = current();
    if (roster->white_close.has_value()) {
        (*roster->white_close)();
    }
    if (roster->black_close.has_value()) {
        (*roster->black_close)();
    }
    for (const Watcher& watcher : roster->watchers) {
        if (watcher.close) {
            watcher.close();
        }
    }
}

}  // namespace kfc::server
