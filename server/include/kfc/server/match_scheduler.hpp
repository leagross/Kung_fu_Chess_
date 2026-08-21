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
class Metrics;

/// Drives many matches from a fixed pool of worker threads (each ticking
/// its own slice 60x/second) instead of one thread per match. add/remove/
/// wake are safe from any thread; a given match is only ever ticked by its
/// own worker, so Match::tick's one-at-a-time requirement holds unlocked.
class MatchScheduler {
public:
    /// worker_count of 0 means one per hardware core.
    explicit MatchScheduler(std::size_t worker_count = 0, Metrics* metrics = nullptr);

    /// Stops and joins every worker. Registered matches are simply dropped.
    ~MatchScheduler();

    MatchScheduler(const MatchScheduler&) = delete;
    MatchScheduler& operator=(const MatchScheduler&) = delete;

    /// Assigns to whichever worker currently holds the fewest matches.
    /// Shared ownership, so a match can't be destroyed mid-tick.
    void add(std::shared_ptr<Match> match);

    /// Unregisters a match; nothing is joined and nothing blocks on the tick
    /// loop. If the owning worker is mid-tick on this match, that tick
    /// finishes normally. Idempotent.
    void remove(const std::shared_ptr<Match>& match);

    /// Nudges the owning worker so a just-enqueued command is applied now
    /// rather than at the next tick. No-op if the match is unknown.
    void wake(const std::shared_ptr<Match>& match);

    [[nodiscard]] std::size_t match_count() const;

    [[nodiscard]] std::size_t worker_count() const { return workers_.size(); }

private:
    struct Worker {
        std::mutex mutex;
        std::condition_variable wakeup;
        std::vector<std::shared_ptr<Match>> matches;
        std::thread thread;
        bool nudged = false;
        // Mirrors matches.size(); readable by add()'s load comparison
        // without locking this worker's mutex.
        std::atomic<std::size_t> load{0};
    };

    void run(Worker& worker);

    Metrics* metrics_;
    std::atomic<bool> running_{true};
    // unique_ptr so adding a worker never relocates a Worker's mutex/thread.
    std::vector<std::unique_ptr<Worker>> workers_;

    // Which worker owns each match, so remove()/wake() are a lookup rather
    // than a scan. Separate mutex from any Worker's own.
    std::mutex owners_mutex_;
    std::unordered_map<Match*, Worker*> owners_;
};

}  // namespace kfc::server
