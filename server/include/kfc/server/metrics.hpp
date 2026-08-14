#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace kfc::server {

/// Process-wide counters for GET /metrics (see HttpApiServer), in Prometheus
/// text exposition format. One instance lives in main() and is threaded down
/// to whatever needs to bump a counter -- today, ClientSession -- so every
/// connection's thread increments the same atomics rather than each keeping
/// its own that would then need summing at scrape time.
///
/// Deliberately holds only counters, not the two gauges an operator would
/// also want (active connections, active rooms): those already have an
/// authoritative source of truth in SessionRegistry::live_count() and
/// RoomManager::room_count(), so render() below is handed both by reference
/// and reads them directly rather than this class trying to track a second,
/// derivable copy that could drift from the first.
///
/// Every counter is a plain std::atomic<std::uint64_t> at relaxed ordering:
/// these are independent tallies, not synchronizing anything else about the
/// object doing the incrementing, and a scrape reading a slightly stale value
/// is exactly as correct as scraping a moment earlier would have been.
class Metrics {
public:
    void message_received() { messages_received_.fetch_add(1, std::memory_order_relaxed); }
    void message_rejected() { messages_rejected_.fetch_add(1, std::memory_order_relaxed); }
    void message_undecodable() { messages_undecodable_.fetch_add(1, std::memory_order_relaxed); }
    void move_processed() { moves_processed_.fetch_add(1, std::memory_order_relaxed); }

    /// One MatchScheduler worker's pass over its due matches -- see
    /// MatchScheduler::run. duration is the wall time of ticking that whole
    /// batch, not a single match's tick, since that batch (not one match) is
    /// the unit of work a worker thread actually does once per interval.
    void record_tick(std::chrono::nanoseconds duration) {
        std::uint64_t ns = static_cast<std::uint64_t>(duration.count());
        tick_count_.fetch_add(1, std::memory_order_relaxed);
        tick_total_ns_.fetch_add(ns, std::memory_order_relaxed);
        // Racing writers only ever move this up: a CAS loop that keeps
        // retrying while another thread's tick is a longer one than the
        // value it just lost to is still moving toward the true max, so a
        // few extra spins under contention are the whole cost, never a wrong
        // answer left behind.
        std::uint64_t prev = tick_max_ns_.load(std::memory_order_relaxed);
        while (ns > prev && !tick_max_ns_.compare_exchange_weak(prev, ns, std::memory_order_relaxed)) {
        }
    }

    [[nodiscard]] std::uint64_t messages_received() const {
        return messages_received_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t messages_rejected() const {
        return messages_rejected_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t messages_undecodable() const {
        return messages_undecodable_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t moves_processed() const {
        return moves_processed_.load(std::memory_order_relaxed);
    }
    [[nodiscard]] std::uint64_t tick_count() const { return tick_count_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t tick_total_ns() const { return tick_total_ns_.load(std::memory_order_relaxed); }
    [[nodiscard]] std::uint64_t tick_max_ns() const { return tick_max_ns_.load(std::memory_order_relaxed); }

    /// Prometheus text exposition format
    /// (https://prometheus.io/docs/instrumenting/exposition_formats/):
    /// active_connections, active_rooms and worker_threads are gauges read
    /// live from their own owners; everything else is one of this object's
    /// own counters. tick_duration is exposed as the idiomatic Prometheus
    /// sum+count pair (avg = rate(sum)/rate(count), computed by whoever
    /// scrapes this, not here) plus a running max gauge, rather than a
    /// histogram -- one bucket-free pair is enough for what stage 3 of
    /// Roadmap.md actually asks for ("avg/max tick duration"), and adding
    /// bucket boundaries with no evidence yet of what a slow tick even looks
    /// like on this codebase would be a guess dressed up as instrumentation.
    /// Content-Type is text/plain; version=0.0.4 -- set by the caller
    /// (HttpApiServer), not here, since that is a transport concern this
    /// class has no other reason to know about.
    [[nodiscard]] std::string render(std::size_t active_connections, std::size_t active_rooms,
                                     std::size_t worker_threads) const;

private:
    std::atomic<std::uint64_t> messages_received_{0};
    std::atomic<std::uint64_t> messages_rejected_{0};
    std::atomic<std::uint64_t> messages_undecodable_{0};
    std::atomic<std::uint64_t> moves_processed_{0};
    std::atomic<std::uint64_t> tick_count_{0};
    std::atomic<std::uint64_t> tick_total_ns_{0};
    std::atomic<std::uint64_t> tick_max_ns_{0};
};

}  // namespace kfc::server
