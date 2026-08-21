#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>

namespace kfc::server {

/// Process-wide counters for GET /metrics (Prometheus text format), shared
/// so every connection thread increments the same atomics. Holds only
/// counters -- gauges like active connections/rooms are read directly from
/// their authoritative source (SessionRegistry, RoomManager) instead.
class Metrics {
public:
    void message_received() { messages_received_.fetch_add(1, std::memory_order_relaxed); }
    void message_rejected() { messages_rejected_.fetch_add(1, std::memory_order_relaxed); }
    void message_undecodable() { messages_undecodable_.fetch_add(1, std::memory_order_relaxed); }
    void move_processed() { moves_processed_.fetch_add(1, std::memory_order_relaxed); }

    /// duration is one MatchScheduler worker's whole pass over its due
    /// matches, not a single match's tick.
    void record_tick(std::chrono::nanoseconds duration) {
        std::uint64_t ns = static_cast<std::uint64_t>(duration.count());
        tick_count_.fetch_add(1, std::memory_order_relaxed);
        tick_total_ns_.fetch_add(ns, std::memory_order_relaxed);
        // CAS loop: only ever moves the max up, so extra spins under
        // contention are the whole cost.
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

    /// active_connections/active_rooms/worker_threads are gauges read live
    /// from their own owners; everything else is this object's own counters.
    /// tick_duration is a Prometheus sum+count pair plus a running max, not a
    /// histogram (no evidence yet of what bucket boundaries should be).
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
