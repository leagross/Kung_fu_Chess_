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

std::string Metrics::render(std::size_t active_connections, std::size_t active_rooms) const {
    std::ostringstream out;
    emit_gauge(out, "kfc_active_connections", "WebSocket connections currently open.", active_connections);
    emit_gauge(out, "kfc_active_rooms", "Rooms currently open (waiting, playing, or not yet reaped).", active_rooms);
    emit_counter(out, "kfc_messages_received_total", "Client frames accepted for decoding.", messages_received());
    emit_counter(out, "kfc_messages_rejected_total",
                "Client frames dropped for being oversized or exceeding the per-connection rate limit.",
                messages_rejected());
    emit_counter(out, "kfc_messages_undecodable_total",
                "Client frames that were neither valid JSON nor a known message type.", messages_undecodable());
    emit_counter(out, "kfc_moves_processed_total", "Move and jump requests routed to a Match.", moves_processed());
    return out.str();
}

}  // namespace kfc::server
