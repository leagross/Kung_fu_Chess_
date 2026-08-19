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

// Records why a seating request failed, for callers that asked. A null pointer
// means the caller doesn't care (tests, mostly) -- the reason is then simply
// not reported, never a crash.
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
    // Destroying the scheduler joins every one of its worker threads, so this
    // does not return until nothing is mid-tick anywhere -- no rooms_mutex_
    // needed for that part, unlike the old per-Match stop(). reset() on an
    // already-null scheduler_ (a second call) is simply a no-op, which is what
    // makes this idempotent.
    //
    // Taken out under scheduler_mutex_ so a concurrent on_disconnect/enqueue
    // either finishes its call into the scheduler before this runs, or sees
    // scheduler_ already null and skips it -- never a call into an object
    // that is mid-destruction (see scheduler_'s own doc comment; found by
    // AddressSanitizer). Destroyed *outside* the lock: joining every worker
    // thread is real time, and holding scheduler_mutex_ across it would stall
    // every connection thread's on_disconnect/enqueue for as long as that
    // takes, on the hot path.
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
    // weak_ptr, not shared_ptr: the hook lives inside the Match it wakes, so a
    // captured shared_ptr would keep it alive forever, itself, via its own
    // wake hook -- a Match that could never be destroyed.
    std::weak_ptr<Match> weak_match = room.match;
    room.match->set_wake_hook([this, weak_match] {
        if (std::shared_ptr<Match> match = weak_match.lock()) {
            // scheduler_mutex_ makes this mutually exclusive with stop_all()
            // taking scheduler_ out to destroy it -- see scheduler_'s own doc
            // comment for why a bare null check was not enough. A wake with
            // nothing left to schedule is simply a no-op.
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
    // Digits and capitals minus the pairs that get misheard or misread when one
    // player reads the id out to another: 0/O, 1/I/L, 5/S, 2/Z, 8/B.
    static constexpr char kAlphabet[] = "34679ACDEFGHJKMNPQRTUVWXY";
    static constexpr int kAlphabetSize = sizeof(kAlphabet) - 1;

    // Six characters: 25^6 = 244 million ids.
    //
    // It was four, which is 390,625 -- and that quietly contradicted the target
    // in Server_Design.md. Five million concurrent games need five million live
    // ids at once, so a four-character id is not merely crowded, it is **1280%
    // full**: there are fewer ids in the whole space than rooms that need one,
    // and the retry loop below would spin forever having exhausted them. Even
    // five characters runs at 51% occupancy, where every id takes two draws on
    // average and the loop is doing as much work as the room.
    //
    // Six leaves the space 2% full at that scale -- 1.02 draws per id, so the
    // retry is the rare event it is written as. The cost is two more characters
    // to read out loud, which is the right trade against an id that cannot be
    // issued at all.
    //
    // If the concurrent-room target ever grows by another two orders of
    // magnitude, this has to grow with it; the arithmetic is
    // rooms / 25^kLength, and it wants to stay a few percent.
    static constexpr int kLength = 6;

    // One generator for the process, seeded once. Not deterministic across runs
    // on purpose: ids that restart from the same sequence every launch would be
    // trivially guessable by anyone who watched a previous session.
    static std::mt19937 generator(std::random_device{}());
    std::uniform_int_distribution<int> pick(0, kAlphabetSize - 1);

    // Retried on the (vanishingly unlikely) collision rather than assumed
    // unique -- an id that silently aliased a live room would send its creator
    // into someone else's game.
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
    // Sorted by rating, so nothing farther out in either direction can beat
    // the two entries immediately adjacent to lower_bound(rating) -- one
    // lookup and at most two comparisons, whatever w (rooms actually waiting)
    // is. Ties favour whichever of the two this happens to check first; both
    // are an equally valid "closest match" (see join_any's own doc comment,
    // which only promises closest by gap, not a tie-break).
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
            // No longer anyone's to be matched with, the instant it's chosen --
            // whether or not the join below actually succeeds (see the "cannot
            // happen" undo path further down, which puts it back if it doesn't).
            unmark_waiting(room_id, target->waiting_rating);
        } else {
            // No compatible opponent waiting -- open a new room and wait there.
            target = &open_room(room_id);
            target->waiting_rating = rating;
            mark_waiting(room_id, rating);
            logger_.log("RoomManager: opened room " + std::to_string(room_id) + " (rating " +
                        std::to_string(rating) + " waiting)");
        }

        // Reserve the seat under the lock so two simultaneous joins can never
        // grab the same last seat. The actual colour still comes from
        // Match::join below -- this counter only gates availability.
        ++target->seats_taken;
        ++target->connected;
        match = target->match;
    }

    // Outside the lock: join()'s Welcome send is network I/O, and enqueue()
    // routing runs under rooms_mutex_ on the hot path -- a join must not block
    // it. The shared_ptr taken under the lock is what keeps the Match alive
    // here, whatever another thread does to the room meanwhile.
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
                    // This was the pairing target above, taken out of
                    // waiting_by_rating_ before the join that just failed --
                    // still one connected player, so it goes back to waiting.
                    mark_waiting(room_id, it->second.waiting_rating);
                }
            }
        }
        if (reaped) {
            // scheduler_mutex_, not a bare null check -- see scheduler_'s own
            // doc comment.
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
        // Minted here, not taken from the client -- so Create always succeeds
        // and two creators can never collide over a name (see CreateRoom).
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
        // Unreachable for a room this call just created, but reported rather
        // than returning a bare nullopt the caller can't explain.
        fail(failure_reason, kfc::protocol::join_reasons::kRoomNotActive);
        return std::nullopt;
    }
    // Outside the lock, like join() above -- a directory write is network I/O
    // in the multi-worker case and must not hold up move routing.
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
                // The name outlived its room -- reaping erases both together,
                // so this shouldn't happen; reported rather than trusted
                // blindly.
                fail(failure_reason, kfc::protocol::join_reasons::kNoSuchRoom);
                return std::nullopt;  // gone
            }
            match = it->second.match;

            // A decided game can't be joined, played in, or usefully watched.
            // Refused with a reason the client can actually show, rather than
            // seating someone into a board that will never move again.
            if (match->is_over()) {
                fail(failure_reason, kfc::protocol::join_reasons::kRoomNotActive);
                return std::nullopt;
            }

            // Is this the player who just dropped, coming back mid-countdown?
            // Then they reclaim their own seat and colour -- no new seat is
            // taken, and the room's connection count simply returns to what
            // it was.
            reclaimed = match->reclaimable_seat_for(username);
            if (!reclaimed.has_value()) {
                // Both seats already taken -> this joiner watches instead.
                // match->join_spectator below is what actually enforces
                // MatchAudience::kMaxSpectators; a full room still falls
                // through this branch and is rejected there, not here.
                as_spectator = it->second.seats_taken >= 2;
                if (!as_spectator) {
                    ++it->second.seats_taken;
                }
            }
            // Counted for every arrival -- player, returning player or
            // viewer: the room must not be reaped while any of them is
            // connected to it.
            ++it->second.connected;
        }
    }

    if (!found_locally) {
        // Not a room this worker knows about. Checked outside rooms_mutex_ --
        // a directory lookup is network I/O in the multi-worker case, and
        // must not hold up move routing for every other room while it runs.
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
        return std::nullopt;  // no room by that name, anywhere this worker can tell
    }

    if (reclaimed.has_value()) {
        if (!match->reconnect(*reclaimed, std::move(send), std::move(close))) {
            // The grace expired in the gap between reclaimable_seat_for saying
            // yes and this call: the match is already forfeit and this seat is
            // nobody's. Give back the connection we counted above, or the room
            // would never reach zero and never be reaped.
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
            // At MatchAudience::kMaxSpectators -- give back the connection
            // counted above, the same way an expired reclaim does, or the
            // room would never reach zero and never be reaped.
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
        // Colour is meaningless for a viewer -- see Seat::spectator.
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
    // The busiest call in the whole server -- every command, from every
    // player, in every room -- so rooms_mutex_ covers only the lookup. Match's
    // own enqueue() is fast (no I/O), but it now also runs the wake hook,
    // which reaches into MatchScheduler's own locks (see match_scheduler.hpp);
    // holding a single mutex shared by every room across that nested locking
    // would have serialized move routing for the entire server behind it,
    // exactly the same reason join_any/on_disconnect already release this
    // lock before touching a Match at all.
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

    // Both done outside the lock -- one hands the event to the Match's own tick
    // thread, the other only edits its audience.
    if (seat.spectator) {
        // A viewer holds no seat, so telling the Match a colour dropped would
        // forfeit an innocent player's game. All that ends is the sending: it
        // stops being part of every broadcast for the rest of the match.
        match->leave_spectator(seat.watcher);
    } else {
        match->on_disconnect(seat.color);
    }

    // Reap the room if that was its last live connection. The Match is
    // unregistered from the scheduler (never joining anything -- see
    // MatchScheduler::remove) only after rooms_mutex_ is released.
    std::shared_ptr<Match> reaped;
    std::string reaped_name;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        auto it = rooms_.find(room);
        if (it != rooms_.end() && --it->second.connected <= 0) {
            // Free the name too (if any) so it can be reused for a new room.
            if (!it->second.name.empty()) {
                named_rooms_.erase(it->second.name);
                reaped_name = it->second.name;
            }
            // Harmless if this room was never in the index (a named room, or
            // one already paired) -- see unmark_waiting's own doc comment.
            unmark_waiting(room, it->second.waiting_rating);
            reaped = std::move(it->second.match);
            rooms_.erase(it);
            logger_.log("RoomManager: closed room " + std::to_string(room));
        }
    }
    if (reaped) {
        // scheduler_ can be null here, or -- the case a bare null check
        // missed -- non-null but *mid-destruction* on the thread running
        // stop_all(): unique_ptr::reset() runs ~MatchScheduler() (which joins
        // every worker thread, taking real time) before it stores nullptr,
        // so a connection thread arriving in that window could still read a
        // non-null pointer and call into an object being torn down
        // underneath it. AddressSanitizer caught both shapes of this at this
        // exact call site. scheduler_mutex_ makes "take scheduler_ out to
        // destroy it" and "look up scheduler_ and call into it" mutually
        // exclusive -- either this call happens before stop_all() starts
        // tearing the scheduler down, or scheduler_ is already null here and
        // there is nothing to unregister from, which is simply skipped
        // rather than crashing (the Match this reaped shared_ptr holds is
        // still cleaned up normally when it goes out of scope).
        {
            std::lock_guard<std::mutex> guard(scheduler_mutex_);
            if (scheduler_) {
                scheduler_->remove(reaped);
            }
        }
        // Outside the lock, same reasoning as register_room -- and best-effort:
        // the directory's own TTL (see RedisRoomDirectory) is what actually
        // guarantees a stale entry doesn't outlive its room forever.
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
