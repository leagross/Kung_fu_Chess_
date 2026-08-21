#include "kfc/server/room_manager.hpp"

#include <cstdlib>
#include <iterator>
#include <random>
#include <string>
#include <utility>

#include "kfc/protocol/file_logger.hpp"
#include "kfc/protocol/messages.hpp"
#include "kfc/database/elo.hpp"

namespace kfc::server {

namespace {

// Null out means the caller doesn't care -- reason is simply not reported.
void fail(std::string* out, const char* reason) {
    if (out != nullptr) {
        *out = reason;
    }
}

}  // namespace

RoomManager::RoomManager(std::function<kfc::model::Board()> board_factory, kfc::protocol::FileLogger& logger,
                         kfc::protocol::GameplayConfig config, ResultCallback on_result, int disconnect_grace_ms,
                         IRoomDirectory* directory, std::string self_url, Metrics* metrics)
    : board_factory_(std::move(board_factory)),
      logger_(logger),
      config_(std::move(config)),
      on_result_(std::move(on_result)),
      disconnect_grace_ms_(disconnect_grace_ms),
      directory_(directory),
      self_url_(std::move(self_url)),
      metrics_(metrics),
      scheduler_(std::make_unique<MatchScheduler>(0, metrics_)) {}

RoomManager::~RoomManager() {
    stop_all();
}

void RoomManager::stop_all() {
    // Taken out under scheduler_mutex_ so a concurrent call never reaches an
    // object mid-destruction; destroyed outside the lock since joining every
    // worker thread is real time and would stall other connections otherwise.
    std::unique_ptr<MatchScheduler> to_destroy;
    {
        std::lock_guard<std::mutex> guard(scheduler_mutex_);
        to_destroy = std::move(scheduler_);
    }
    to_destroy.reset();
}

RoomManager::Room& RoomManager::open_room(RoomId& id_out, std::string room_name) {
    id_out = next_room_id_++;
    Room room;
    room.match = std::make_shared<Match>(board_factory_(), logger_, config_, on_result_, disconnect_grace_ms_,
                                          kDefaultReleaseDelayMs, room_name);
    room.name = std::move(room_name);
    // weak_ptr: a captured shared_ptr would keep the Match alive forever via
    // its own wake hook.
    std::weak_ptr<Match> weak_match = room.match;
    room.match->set_wake_hook([this, weak_match] {
        if (std::shared_ptr<Match> match = weak_match.lock()) {
            std::lock_guard<std::mutex> guard(scheduler_mutex_);
            if (scheduler_) {
                scheduler_->wake(match);
            }
        }
    });
    {
        std::lock_guard<std::mutex> guard(scheduler_mutex_);
        if (scheduler_) {
            scheduler_->add(room.match);
        }
    }
    return rooms_.emplace(id_out, std::move(room)).first->second;
}

std::string RoomManager::generate_room_id() {
    // Digits and capitals minus pairs easily misheard/misread: 0/O, 1/I/L, 5/S, 2/Z, 8/B.
    static constexpr char kAlphabet[] = "34679ACDEFGHJKMNPQRTUVWXY";
    static constexpr int kAlphabetSize = sizeof(kAlphabet) - 1;

    // 25^6 = 244 million ids, kept a few percent full at the target concurrent
    // room count so the retry loop below stays rare.
    static constexpr int kLength = 6;

    // Not deterministic across runs, so ids aren't guessable from a past session.
    static std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> pick(0, kAlphabetSize - 1);

    while (true) {
        std::string id(kLength, '\0');
        for (char& c : id) {
            c = kAlphabet[pick(generator)];
        }
        if (named_rooms_.count(id) == 0) {
            return id;
        }
    }
}

std::optional<RoomId> RoomManager::closest_waiting_room(int rating) const {
    // Sorted by rating: nothing farther out can beat the two entries
    // adjacent to lower_bound(rating).
    std::optional<std::pair<int, RoomId>> best;  // {gap, room}
    auto consider = [&](std::multimap<int, RoomId>::const_iterator it) {
        if (it == waiting_by_rating_.end()) {
            return;
        }
        int gap = std::abs(it->first - rating);
        if (gap <= kfc::database::kMatchmakingRatingGap && (!best.has_value() || gap < best->first)) {
            best = std::make_pair(gap, it->second);
        }
    };

    auto lower = waiting_by_rating_.lower_bound(rating);
    consider(lower);
    if (lower != waiting_by_rating_.begin()) {
        consider(std::prev(lower));
    }
    if (!best.has_value()) {
        return std::nullopt;
    }
    return best->second;
}

void RoomManager::mark_waiting(RoomId id, int rating) {
    waiting_by_rating_.emplace(rating, id);
}

void RoomManager::unmark_waiting(RoomId id, int rating) {
    auto [begin, end] = waiting_by_rating_.equal_range(rating);
    for (auto it = begin; it != end; ++it) {
        if (it->second == id) {
            waiting_by_rating_.erase(it);
            return;
        }
    }
}

std::optional<RoomManager::Seat> RoomManager::join_any(const std::string& username, int rating, SendFn send,
                                                        CloseFn close) {
    RoomId room_id = 0;
    std::shared_ptr<Match> match;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);

        std::optional<RoomId> found = closest_waiting_room(rating);
        Room* target = nullptr;
        if (found.has_value()) {
            room_id = *found;
            target = &rooms_.at(room_id);
            unmark_waiting(room_id, target->waiting_rating);
        } else {
            target = &open_room(room_id);
            target->waiting_rating = rating;
            mark_waiting(room_id, rating);
            logger_.log("RoomManager: opened room " + std::to_string(room_id) + " (rating " +
                        std::to_string(rating) + " waiting)");
        }

        // Reserved under the lock so two simultaneous joins can't grab the
        // same last seat; colour itself comes from Match::join below.
        ++target->seats_taken;
        ++target->connected;
        match = target->match;
    }

    // Outside the lock: join()'s Welcome send is network I/O and must not
    // block enqueue()'s routing. The shared_ptr taken above keeps the Match alive.
    std::optional<kfc::model::PieceColor> color = match->join(username, rating, std::move(send), std::move(close));
    if (!color.has_value()) {
        // Cannot happen -- seats_taken caps a room at two joiners -- but undo
        // the reservation defensively and reap the room if it's now empty.
        std::shared_ptr<Match> reaped;
        {
            std::lock_guard<std::mutex> guard(rooms_mutex_);
            auto it = rooms_.find(room_id);
            if (it != rooms_.end()) {
                --it->second.seats_taken;
                if (--it->second.connected <= 0) {
                    unmark_waiting(room_id, it->second.waiting_rating);
                    reaped = std::move(it->second.match);
                    rooms_.erase(it);
                } else if (it->second.seats_taken == 1) {
                    mark_waiting(room_id, it->second.waiting_rating);
                }
            }
        }
        if (reaped) {
            std::lock_guard<std::mutex> guard(scheduler_mutex_);
            if (scheduler_) {
                scheduler_->remove(reaped);
            }
        }
        return std::nullopt;
    }

    logger_.log("RoomManager: '" + username + "' joined room " + std::to_string(room_id));
    return Seat{room_id, *color};
}

std::optional<RoomManager::Seat> RoomManager::create_room(const std::string& username, int rating, SendFn send,
                                                          CloseFn close, std::string* failure_reason) {
    RoomId room_id = 0;
    std::shared_ptr<Match> match;
    std::string room_key;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        // Minted here, not taken from the client, so Create can't collide over a name.
        room_key = generate_room_id();
        Room& room = open_room(room_id, room_key);
        named_rooms_[room_key] = room_id;
        ++room.seats_taken;
        ++room.connected;
        match = room.match;
        logger_.log("RoomManager: created room '" + room_key + "' (id " + std::to_string(room_id) + ") for '" +
                    username + "'");
    }

    std::optional<kfc::model::PieceColor> color = match->join(username, rating, std::move(send), std::move(close));
    if (!color.has_value()) {
        fail(failure_reason, kfc::protocol::join_reasons::kRoomNotActive);
        return std::nullopt;
    }
    // Outside the lock: a directory write is network I/O.
    if (directory_ != nullptr) {
        directory_->register_room(room_key, self_url_);
    }
    return Seat{room_id, *color};  // White
}

std::optional<RoomManager::Seat> RoomManager::join_room(const std::string& name, const std::string& username,
                                                        int rating, SendFn send, CloseFn close,
                                                        std::string* failure_reason, std::string* redirect_url) {
    RoomId room_id = 0;
    std::shared_ptr<Match> match;
    bool as_spectator = false;
    bool found_locally = false;
    std::optional<kfc::model::PieceColor> reclaimed;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        auto named = named_rooms_.find(name);
        found_locally = named != named_rooms_.end();
        if (found_locally) {
            room_id = named->second;
            auto it = rooms_.find(room_id);
            if (it == rooms_.end()) {
                fail(failure_reason, kfc::protocol::join_reasons::kNoSuchRoom);
                return std::nullopt;  // gone
            }
            match = it->second.match;

            if (match->is_over()) {
                fail(failure_reason, kfc::protocol::join_reasons::kRoomNotActive);
                return std::nullopt;
            }

            reclaimed = match->reclaimable_seat_for(username);
            if (!reclaimed.has_value()) {
                // match->join_spectator below is what actually enforces
                // MatchAudience::kMaxSpectators.
                as_spectator = it->second.seats_taken >= 2;
                if (!as_spectator) {
                    ++it->second.seats_taken;
                }
            }
            ++it->second.connected;
        }
    }

    if (!found_locally) {
        // Directory lookup is network I/O; checked outside rooms_mutex_ so
        // it doesn't hold up routing for every other room.
        if (directory_ != nullptr) {
            std::optional<std::string> owner = directory_->owner_of(name);
            if (owner.has_value()) {
                if (redirect_url != nullptr) {
                    *redirect_url = *owner;
                }
                return std::nullopt;  // caller sends JoinRedirect, not JoinFailed
            }
        }
        fail(failure_reason, kfc::protocol::join_reasons::kNoSuchRoom);
        return std::nullopt;
    }

    if (reclaimed.has_value()) {
        if (!match->reconnect(*reclaimed, std::move(send), std::move(close))) {
            // Grace expired between reclaimable_seat_for saying yes and this
            // call. Give back the connection counted above, or the room
            // would never be reaped.
            {
                std::lock_guard<std::mutex> guard(rooms_mutex_);
                auto it = rooms_.find(room_id);
                if (it != rooms_.end()) {
                    --it->second.connected;
                }
            }
            fail(failure_reason, kfc::protocol::join_reasons::kRoomNotActive);
            return std::nullopt;
        }
        logger_.log("RoomManager: '" + username + "' returned to room '" + name + "'");
        return Seat{room_id, *reclaimed};
    }

    if (as_spectator) {
        WatcherId watcher = match->join_spectator(username, std::move(send), std::move(close));
        if (watcher == 0) {
            // At MatchAudience::kMaxSpectators; give back the connection counted above.
            {
                std::lock_guard<std::mutex> guard(rooms_mutex_);
                auto it = rooms_.find(room_id);
                if (it != rooms_.end()) {
                    --it->second.connected;
                }
            }
            fail(failure_reason, kfc::protocol::join_reasons::kSpectatorLimitReached);
            return std::nullopt;
        }
        logger_.log("RoomManager: '" + username + "' is watching room '" + name + "'");
        return Seat{room_id, kfc::model::PieceColor::White, /*spectator=*/true, watcher};
    }

    std::optional<kfc::model::PieceColor> color = match->join(username, rating, std::move(send), std::move(close));
    if (!color.has_value()) {
        return std::nullopt;
    }
    logger_.log("RoomManager: '" + username + "' joined room '" + name + "'");
    return Seat{room_id, *color};  // Black
}

void RoomManager::enqueue(RoomId room, kfc::model::PieceColor from, kfc::protocol::ClientMessage message) {
    // rooms_mutex_ covers only the lookup: Match::enqueue() also runs the
    // wake hook, which reaches into MatchScheduler's own locks, and holding
    // this mutex across that would serialize routing for every room behind it.
    std::shared_ptr<Match> match;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        auto it = rooms_.find(room);
        if (it == rooms_.end()) {
            return;
        }
        match = it->second.match;
    }
    match->enqueue(from, std::move(message));
}

void RoomManager::on_disconnect(const Seat& seat) {
    RoomId room = seat.room;
    std::shared_ptr<Match> match;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        auto it = rooms_.find(room);
        if (it == rooms_.end()) {
            return;
        }
        match = it->second.match;
    }

    if (seat.spectator) {
        match->leave_spectator(seat.watcher);
    } else {
        match->on_disconnect(seat.color);
    }

    // Reap the room if that was its last live connection.
    std::shared_ptr<Match> reaped;
    std::string reaped_name;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        auto it = rooms_.find(room);
        if (it != rooms_.end() && --it->second.connected <= 0) {
            if (!it->second.name.empty()) {
                named_rooms_.erase(it->second.name);
                reaped_name = it->second.name;
            }
            unmark_waiting(room, it->second.waiting_rating);
            reaped = std::move(it->second.match);
            rooms_.erase(it);
            logger_.log("RoomManager: closed room " + std::to_string(room));
        }
    }
    if (reaped) {
        // scheduler_mutex_ makes "take scheduler_ out to destroy it" and
        // "look up scheduler_ and call into it" mutually exclusive, so this
        // never calls into a MatchScheduler mid-teardown (see scheduler_'s
        // own doc comment).
        {
            std::lock_guard<std::mutex> guard(scheduler_mutex_);
            if (scheduler_) {
                scheduler_->remove(reaped);
            }
        }
        // Best-effort: the directory's own TTL is what actually guarantees a
        // stale entry doesn't outlive its room forever.
        if (directory_ != nullptr && !reaped_name.empty()) {
            directory_->forget_room(reaped_name);
        }
    }
}

std::size_t RoomManager::room_count() const {
    std::lock_guard<std::mutex> guard(rooms_mutex_);
    return rooms_.size();
}

std::size_t RoomManager::worker_count() const {
    std::lock_guard<std::mutex> guard(scheduler_mutex_);
    return scheduler_ ? scheduler_->worker_count() : 0;
}

}  // namespace kfc::server
