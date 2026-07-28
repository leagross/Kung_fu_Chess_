#include "kfc/server/room_manager.hpp"

#include <cstdlib>
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
                         kfc::protocol::GameplayConfig config, ResultCallback on_result, int disconnect_grace_ms)
    : board_factory_(std::move(board_factory)),
      logger_(logger),
      config_(std::move(config)),
      on_result_(std::move(on_result)),
      disconnect_grace_ms_(disconnect_grace_ms) {}

RoomManager::~RoomManager() {
    // Stop every room's tick thread before the Matches are destroyed. ~Match
    // also calls stop(), but stopping them all here first keeps teardown order
    // explicit and joins each thread while the map is still intact.
    std::lock_guard<std::mutex> guard(rooms_mutex_);
    for (auto& [id, room] : rooms_) {
        room.match->stop();
    }
}

RoomManager::Room& RoomManager::open_room(RoomId& id_out, std::string room_name) {
    id_out = next_room_id_++;
    Room room;
    room.match = std::make_shared<Match>(board_factory_(), logger_, config_, on_result_, disconnect_grace_ms_,
                                          kDefaultReleaseDelayMs, room_name);
    room.name = std::move(room_name);
    room.match->start();
    return rooms_.emplace(id_out, std::move(room)).first->second;
}

std::string RoomManager::generate_room_id() {
    // Digits and capitals minus the pairs that get misheard or misread when one
    // player reads the id out to another: 0/O, 1/I/L, 5/S, 2/Z, 8/B.
    static constexpr char kAlphabet[] = "34679ACDEFGHJKMNPQRTUVWXY";
    static constexpr int kAlphabetSize = sizeof(kAlphabet) - 1;
    static constexpr int kLength = 4;  // 25^4 ~ 390k -- plenty, and still short

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

std::optional<RoomManager::Seat> RoomManager::join_any(const std::string& username, int rating, SendFn send,
                                                        CloseFn close) {
    RoomId room_id = 0;
    std::shared_ptr<Match> match;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);

        // Find the waiting room whose lone player is closest in rating and
        // within the matchmaking gap. A room with two seats already taken is a
        // game in progress, never a match candidate.
        Room* target = nullptr;
        int best_gap = kfc::database::kMatchmakingRatingGap + 1;
        for (auto& [id, room] : rooms_) {
            if (room.seats_taken != 1) {
                continue;
            }
            int gap = std::abs(room.waiting_rating - rating);
            if (gap <= kfc::database::kMatchmakingRatingGap && gap < best_gap) {
                best_gap = gap;
                target = &room;
                room_id = id;
            }
        }
        if (target == nullptr) {
            // No compatible opponent waiting -- open a new room and wait there.
            target = &open_room(room_id);
            target->waiting_rating = rating;
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
    std::optional<kfc::model::PieceColor> color = match->join(username, std::move(send), std::move(close));
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
                    reaped = std::move(it->second.match);
                    rooms_.erase(it);
                }
            }
        }
        if (reaped) {
            reaped->stop();
        }
        return std::nullopt;
    }

    logger_.log("RoomManager: '" + username + "' joined room " + std::to_string(room_id));
    return Seat{room_id, *color};
}

std::optional<RoomManager::Seat> RoomManager::create_room(const std::string& username, SendFn send, CloseFn close,
                                                          std::string* failure_reason) {
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

    std::optional<kfc::model::PieceColor> color = match->join(username, std::move(send), std::move(close));
    if (!color.has_value()) {
        // Unreachable for a room this call just created, but reported rather
        // than returning a bare nullopt the caller can't explain.
        fail(failure_reason, kfc::protocol::join_reasons::kRoomNotActive);
        return std::nullopt;
    }
    return Seat{room_id, *color};  // White
}

std::optional<RoomManager::Seat> RoomManager::join_room(const std::string& name, const std::string& username,
                                                        SendFn send, CloseFn close, std::string* failure_reason) {
    RoomId room_id = 0;
    std::shared_ptr<Match> match;
    bool as_spectator = false;
    std::optional<kfc::model::PieceColor> reclaimed;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        auto named = named_rooms_.find(name);
        if (named == named_rooms_.end()) {
            fail(failure_reason, kfc::protocol::join_reasons::kNoSuchRoom);
            return std::nullopt;  // no room by that name
        }
        room_id = named->second;
        auto it = rooms_.find(room_id);
        if (it == rooms_.end()) {
            // The name outlived its room -- reaping erases both together, so
            // this shouldn't happen; reported rather than trusted blindly.
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

        // Is this the player who just dropped, coming back mid-countdown? Then
        // they reclaim their own seat and colour -- no new seat is taken, and
        // the room's connection count simply returns to what it was.
        reclaimed = match->reclaimable_seat_for(username);
        if (!reclaimed.has_value()) {
            // Both seats already taken -> this joiner watches instead. Viewers
            // are unlimited, so there is no rejection path left here.
            as_spectator = it->second.seats_taken >= 2;
            if (!as_spectator) {
                ++it->second.seats_taken;
            }
        }
        // Counted for every arrival -- player, returning player or viewer: the
        // room must not be reaped while any of them is connected to it.
        ++it->second.connected;
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
        logger_.log("RoomManager: '" + username + "' is watching room '" + name + "'");
        // Colour is meaningless for a viewer -- see Seat::spectator.
        return Seat{room_id, kfc::model::PieceColor::White, /*spectator=*/true, watcher};
    }

    std::optional<kfc::model::PieceColor> color = match->join(username, std::move(send), std::move(close));
    if (!color.has_value()) {
        return std::nullopt;
    }
    logger_.log("RoomManager: '" + username + "' joined room '" + name + "'");
    return Seat{room_id, *color};  // Black
}

void RoomManager::enqueue(RoomId room, kfc::model::PieceColor from, kfc::protocol::ClientMessage message) {
    std::lock_guard<std::mutex> guard(rooms_mutex_);
    auto it = rooms_.find(room);
    if (it != rooms_.end()) {
        it->second.match->enqueue(from, std::move(message));
    }
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

    // Reap the room if that was its last live connection. The Match is stopped
    // (which joins its tick thread) only after rooms_mutex_ is released.
    std::shared_ptr<Match> reaped;
    {
        std::lock_guard<std::mutex> guard(rooms_mutex_);
        auto it = rooms_.find(room);
        if (it != rooms_.end() && --it->second.connected <= 0) {
            // Free the name too (if any) so it can be reused for a new room.
            if (!it->second.name.empty()) {
                named_rooms_.erase(it->second.name);
            }
            reaped = std::move(it->second.match);
            rooms_.erase(it);
            logger_.log("RoomManager: closed room " + std::to_string(room));
        }
    }
    if (reaped) {
        reaped->stop();
    }
}

std::size_t RoomManager::room_count() const {
    std::lock_guard<std::mutex> guard(rooms_mutex_);
    return rooms_.size();
}

}  // namespace kfc::server
