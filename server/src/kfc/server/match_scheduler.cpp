#include "kfc/server/match_scheduler.hpp"

#include <algorithm>
#include <chrono>
#include <utility>

#include "kfc/server/match.hpp"
#include "kfc/server/metrics.hpp"

namespace kfc::server {

namespace {

constexpr int kTickIntervalMs = 16;  // ~60 Hz

// Clamped so elapsed time is never 0 (simulation must move forward) and
// never huge (a stalled process can't dump seconds into one tick).
constexpr int kMinTickAdvanceMs = 1;
constexpr int kMaxTickAdvanceMs = 200;

}  // namespace

MatchScheduler::MatchScheduler(std::size_t worker_count, Metrics* metrics) : metrics_(metrics) {
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
    // Started only once every Worker exists, so run() never sees a partially-filled vector.
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
    // Captured before match is moved-from below.
    Match* key = match.get();

    // Least-loaded worker, from each Worker::load (atomic, no locking needed).
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
    // Nothing joined: if the worker is mid-tick on this match, it holds its
    // own shared_ptr copy for the duration (see run), so this returns immediately.
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

        // Copied out under the lock, then ticked without it: a tick's
        // broadcast can re-enter this scheduler (disconnect -> reap ->
        // remove), which would deadlock if the worker's mutex were still held.
        std::vector<std::shared_ptr<Match>> due;
        {
            std::lock_guard<std::mutex> guard(worker.mutex);
            due = worker.matches;
        }
        auto pass_started_at = std::chrono::steady_clock::now();
        for (const std::shared_ptr<Match>& match : due) {
            match->tick(now, elapsed_ms);
        }
        if (metrics_ != nullptr) {
            metrics_->record_tick(std::chrono::steady_clock::now() - pass_started_at);
        }
    }
}

}  // namespace kfc::server
