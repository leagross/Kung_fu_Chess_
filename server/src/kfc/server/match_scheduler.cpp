#include "kfc/server/match_scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "kfc/server/match.hpp"

namespace kfc::server {

namespace {

// The cadence every match is advanced at -- the same ~60 Hz a per-match thread
// used to run at, so nothing about the simulation's feel changes.
constexpr int kTickIntervalMs = 16;

// Real elapsed time is clamped before it reaches a match: never 0, so the
// simulation always moves forward, and never a large jump, so a stalled process
// cannot dump seconds of simulated time into one tick.
constexpr int kMinTickAdvanceMs = 1;
constexpr int kMaxTickAdvanceMs = 200;

}  // namespace

MatchScheduler::MatchScheduler(std::size_t worker_count) {
    if (worker_count == 0) {
        worker_count = std::thread::hardware_concurrency();
        if (worker_count == 0) {
            worker_count = 1;  // hardware_concurrency is allowed to not know
        }
    }

    workers_.reserve(worker_count);
    for (std::size_t i = 0; i < worker_count; ++i) {
        workers_.push_back(std::make_unique<Worker>());
    }
    // Threads start only once every Worker exists: a worker's run loop reads
    // running_ and its own state, and starting them during the loop above would
    // have them running while the vector was still being filled.
    for (std::unique_ptr<Worker>& worker : workers_) {
        worker->thread = std::thread([this, w = worker.get()] { run(*w); });
    }
}

MatchScheduler::~MatchScheduler() {
    running_.store(false);
    for (std::unique_ptr<Worker>& worker : workers_) {
        {
            std::lock_guard<std::mutex> guard(worker->mutex);
            worker->nudged = true;  // so a worker waiting out its interval leaves now
        }
        worker->wakeup.notify_all();
    }
    for (std::unique_ptr<Worker>& worker : workers_) {
        if (worker->thread.joinable()) {
            worker->thread.join();
        }
    }
}

void MatchScheduler::add(std::shared_ptr<Match> match) {
    if (!match) {
        return;
    }
    // Captured before match is moved-from below -- this is the map key, and a
    // shared_ptr that has been moved out of no longer has a usable .get().
    Match* key = match.get();

    // Least-loaded worker, decided from each Worker::load alone -- an atomic
    // read, so choosing among sixteen workers locks none of them. Rooms come
    // and go constantly (83,000 a second at the target scale), so an even
    // spread matters more than any affinity: a worker that happened to
    // collect the long games would fall behind while its neighbours idled.
    Worker* chosen = nullptr;
    std::size_t fewest = 0;
    for (std::unique_ptr<Worker>& worker : workers_) {
        std::size_t held = worker->load.load(std::memory_order_relaxed);
        if (chosen == nullptr || held < fewest) {
            chosen = worker.get();
            fewest = held;
        }
    }
    if (chosen == nullptr) {
        return;
    }

    {
        std::lock_guard<std::mutex> guard(chosen->mutex);
        chosen->matches.push_back(std::move(match));
    }
    chosen->load.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> guard(owners_mutex_);
        owners_[key] = chosen;
    }
    chosen->wakeup.notify_one();
}

void MatchScheduler::remove(const std::shared_ptr<Match>& match) {
    if (!match) {
        return;
    }
    // Which worker, via one map lookup -- not the sixteen-mutex scan this
    // used to be (see owner_of's old body, and the class comment on why that
    // mattered). Erased here regardless of whether the vector erase below
    // actually finds anything, since either way this match is no longer this
    // scheduler's to own.
    Worker* owner = nullptr;
    {
        std::lock_guard<std::mutex> guard(owners_mutex_);
        auto found = owners_.find(match.get());
        if (found == owners_.end()) {
            return;
        }
        owner = found->second;
        owners_.erase(found);
    }

    std::lock_guard<std::mutex> guard(owner->mutex);
    auto it = std::find(owner->matches.begin(), owner->matches.end(), match);
    if (it != owner->matches.end()) {
        owner->matches.erase(it);
        owner->load.fetch_sub(1, std::memory_order_relaxed);
    }
    // Nothing joined, nothing waited for. If the worker is mid-tick on this
    // match right now it holds its own shared_ptr copy for the duration (see
    // run), so the object outlives the tick and this call returns immediately.
}

void MatchScheduler::wake(const std::shared_ptr<Match>& match) {
    Worker* worker = nullptr;
    {
        std::lock_guard<std::mutex> guard(owners_mutex_);
        auto found = owners_.find(match.get());
        if (found == owners_.end()) {
            return;
        }
        worker = found->second;
    }
    {
        std::lock_guard<std::mutex> guard(worker->mutex);
        worker->nudged = true;
    }
    worker->wakeup.notify_one();
}

std::size_t MatchScheduler::match_count() const {
    // Each Worker::load, summed lock-free -- see match_count's own doc on why
    // "for tests and diagnostics" never needed the stronger guarantee a lock
    // on every worker would have bought it.
    std::size_t total = 0;
    for (const std::unique_ptr<Worker>& worker : workers_) {
        total += worker->load.load(std::memory_order_relaxed);
    }
    return total;
}

void MatchScheduler::run(Worker& worker) {
    auto last_tick_at = std::chrono::steady_clock::now();

    while (running_.load()) {
        {
            std::unique_lock<std::mutex> lock(worker.mutex);
            // Wait out the interval, or leave early when a command arrives or the
            // scheduler is shutting down.
            worker.wakeup.wait_for(lock, std::chrono::milliseconds(kTickIntervalMs),
                                   [this, &worker] { return worker.nudged || !running_.load(); });
            worker.nudged = false;
        }
        if (!running_.load()) {
            return;
        }

        auto now = std::chrono::steady_clock::now();
        int elapsed_ms =
            static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_at).count());
        last_tick_at = now;
        elapsed_ms = std::clamp(elapsed_ms, kMinTickAdvanceMs, kMaxTickAdvanceMs);

        // Copied out under the lock, then ticked without it. A tick broadcasts,
        // and a broadcast can come back into this scheduler (a close arrives as
        // a disconnect, which reaps a room, which calls remove) -- holding the
        // worker's mutex across that would deadlock against ourselves. The copy
        // is of shared_ptrs, so a match removed mid-pass stays alive until this
        // pass is done with it.
        std::vector<std::shared_ptr<Match>> due;
        {
            std::lock_guard<std::mutex> guard(worker.mutex);
            due = worker.matches;
        }
        for (const std::shared_ptr<Match>& match : due) {
            match->tick(now, elapsed_ms);
        }
    }
}

}  // namespace kfc::server
