#pragma once

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>

namespace kfc::server {

class Match;

/// Drives many matches from a few threads.
///
/// **Why this exists.** A `Match` used to own a `std::thread`. That is the right
/// shape for one game and impossible for a million: Server_Design.md's target of
/// five million concurrent games would need five million threads, which at even
/// 64 KB of committed stack each is 320 GB before any chess is played -- and
/// every one of them waking sixty times a second is 300 million wakeups per
/// second, against the roughly one million context switches per second a core
/// can actually do. The work per room is small; it was never the work that did
/// not scale, it was the thread.
///
/// So the threads are counted in cores rather than in games. Each worker owns a
/// slice of the matches and calls `Match::tick` on each in turn, sixty times a
/// second. Sixteen workers replace five million threads and 960 wakeups replace
/// 300 million, while the CPU spent per room is unchanged.
///
/// **It also removes a deadlock, by removing the thing that caused it.** Reaping
/// an empty room used to mean `Match::stop()`, which *joined* that match's
/// thread -- and it was called from inside an IXWebSocket connection callback,
/// while the joined thread could be inside a `send()` on sockets the same
/// library was closing. `remove()` here only unregisters; there is no thread to
/// join, so the join cannot happen from the wrong place.
///
/// **Latency is preserved deliberately.** Dropping the per-match
/// `condition_variable` would have meant a command waiting up to a full tick;
/// instead each *worker* has one, and `wake()` nudges it, so a move is still
/// applied as soon as it lands. Sixteen condition variables instead of five
/// million, with the same responsiveness -- an "optimisation" that made the game
/// feel slower would not be one.
///
/// Threading: `add`, `remove` and `wake` are safe from any thread (connection
/// threads call them). A given match is only ever ticked by its own worker, so
/// `Match::tick`'s one-thread-at-a-time requirement holds without a lock around
/// it.
///
/// **Locking is O(1) per call, not O(workers).** `wake()` runs once per
/// enqueued command -- every move, every player, the single hottest path in
/// the whole server -- and `add()` runs once per room opened, which
/// generate_room_id's own comment puts at 83,000 a second at the target
/// scale. Both used to lock every worker in turn (`add` to compare load,
/// `wake`/`remove` to find the right one via `owner_of`'s linear scan) before
/// touching just one of them; at that call rate, locking fifteen mutexes to
/// use the sixteenth is exactly the kind of lock cost worth not paying twice.
/// `owners_` (its own small mutex, held only for a map lookup/insert/erase)
/// answers "which worker owns this match" directly, and each `Worker::load`
/// is an atomic read `add()` compares without locking anything -- only the
/// one worker actually chosen is ever locked.
class MatchScheduler {
public:
    /// worker_count of 0 means "one per hardware core", which is the sizing the
    /// arithmetic above assumes. Workers start immediately and idle until a
    /// match is added.
    explicit MatchScheduler(std::size_t worker_count = 0);

    /// Stops every worker and joins them. Matches still registered are simply
    /// dropped -- stopping the scheduler is the end of all of them.
    ~MatchScheduler();

    MatchScheduler(const MatchScheduler&) = delete;
    MatchScheduler& operator=(const MatchScheduler&) = delete;

    /// Registers a match to be ticked, on whichever worker currently holds the
    /// fewest (compared via each `Worker::load` -- an atomic read, no locking
    /// needed to make sixteen comparisons). The scheduler shares ownership, so
    /// a match cannot be destroyed while a worker might still be part-way
    /// through ticking it.
    void add(std::shared_ptr<Match> match);

    /// Unregisters a match. It stops being ticked; **nothing is joined and
    /// nothing blocks on the tick loop**, which is the whole point (see above).
    ///
    /// If the owning worker is mid-tick on this very match, that tick finishes
    /// normally -- the scheduler's shared_ptr keeps the object alive until it
    /// does. Idempotent, and harmless for a match that was never added.
    void remove(const std::shared_ptr<Match>& match);

    /// Nudges the worker that owns match so a command just enqueued is applied
    /// now rather than at the next tick. A no-op if the match is unknown.
    ///
    /// This is the call every enqueued command makes, so finding the owning
    /// worker is an `owners_` map lookup, not a scan of every worker's
    /// matches.
    void wake(const std::shared_ptr<Match>& match);

    /// How many matches are registered -- for tests and diagnostics.
    [[nodiscard]] std::size_t match_count() const;

    /// How many worker threads are running.
    [[nodiscard]] std::size_t worker_count() const { return workers_.size(); }

private:
    // One worker: a thread, the matches it owns, and the condition variable that
    // both paces it and lets a command wake it early.
    struct Worker {
        std::mutex mutex;
        std::condition_variable wakeup;
        std::vector<std::shared_ptr<Match>> matches;
        std::thread thread;
        bool nudged = false;
        // Mirrors matches.size(), but readable by add()'s load comparison
        // without locking this worker's mutex -- see the class comment on
        // why that matters at 83,000 add()s a second. Only ever wrong for the
        // instant between a push_back/erase and this catching up, which is
        // fine: the number picks a reasonably-loaded worker, not an exact one.
        std::atomic<std::size_t> load{0};
    };

    void run(Worker& worker);

    std::atomic<bool> running_{true};
    // unique_ptr so adding a worker never moves the others: a Worker holds a
    // mutex and a thread, neither of which can be relocated.
    std::vector<std::unique_ptr<Worker>> workers_;

    // Which worker owns each registered match, so remove()/wake() are a
    // lookup rather than a scan of every worker in turn. A separate small
    // mutex rather than piggybacking on any Worker's -- looking up owner
    // and locking that worker are two different critical sections, and
    // never need to be one. Only add()/remove() write it, from any thread.
    std::mutex owners_mutex_;
    std::unordered_map<Match*, Worker*> owners_;
};

}  // namespace kfc::server
