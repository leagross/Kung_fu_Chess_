#include "kfc/server/metrics.hpp"

#include <sstream>

namespace kfc::server {

namespace {

void emit_gauge(std::ostringstream& out, const char* name, const char* help, std::uint64_t value) {
    out << "# HELP " << name << ' ' << help << '\n';
    out << "# TYPE " << name << " gauge\n";
    out << name << ' ' << value << '\n';
}

void emit_counter(std::ostringstream& out, const char* name, const char* help, std::uint64_t value) {
    out << "# HELP " << name << ' ' << help << '\n';
    out << "# TYPE " << name << " counter\n";
    out << name << ' ' << value << '\n';
}

}  // namespace

std::string Metrics::render(std::size_t active_connections, std::size_t active_rooms,
                            std::size_t worker_threads) const {
    std::ostringstream out;
    emit_gauge(out, "kfc_active_connections", "WebSocket connections currently open.", active_connections);
    emit_gauge(out, "kfc_active_rooms", "Rooms currently open (waiting, playing, or not yet reaped).", active_rooms);
    emit_gauge(out, "kfc_worker_threads", "MatchScheduler worker threads ticking those rooms.", worker_threads);
    emit_counter(out, "kfc_messages_received_total", "Client frames accepted for decoding.", messages_received());
    emit_counter(out, "kfc_messages_rejected_total",
                "Client frames dropped for being oversized or exceeding the per-connection rate limit.",
                messages_rejected());
    emit_counter(out, "kfc_messages_undecodable_total",
                "Client frames that were neither valid JSON nor a known message type.", messages_undecodable());
    emit_counter(out, "kfc_moves_processed_total", "Move and jump requests routed to a Match.", moves_processed());

    // Seconds, not the nanoseconds record_tick stores internally -- Prometheus
    // convention for a *_seconds metric, and floating point so a sum well
    // under a second (the expected case) still shows as a real number rather
    // than truncating to 0.
    double total_seconds = static_cast<double>(tick_total_ns()) / 1e9;
    double max_seconds = static_cast<double>(tick_max_ns()) / 1e9;
    out << "# HELP kfc_tick_duration_seconds_sum Total wall time MatchScheduler workers have spent ticking their "
          "due matches.\n";
    out << "# TYPE kfc_tick_duration_seconds_sum counter\n";
    out << "kfc_tick_duration_seconds_sum " << total_seconds << '\n';
    emit_counter(out, "kfc_tick_duration_seconds_count", "How many worker tick passes that sum is over.",
                tick_count());
    out << "# HELP kfc_tick_duration_seconds_max Longest single worker tick pass observed since this process "
          "started.\n";
    out << "# TYPE kfc_tick_duration_seconds_max gauge\n";
    out << "kfc_tick_duration_seconds_max " << max_seconds << '\n';

    return out.str();
}

}  // namespace kfc::server
