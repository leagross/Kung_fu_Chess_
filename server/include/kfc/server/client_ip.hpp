#pragma once

#include <optional>
#include <string>
#include <unordered_set>

namespace kfc::server {

/// Resolves the address a request should be rate-limited/logged under. In production the TCP peer
/// is Caddy (see DOCKER/Caddyfile), not the actual client, so trusting peer_ip alone would bucket
/// every real client together and make the auth rate limiter useless. Caddy appends the connection
/// it actually received to X-Forwarded-For, so once peer_ip is a configured trusted proxy the last
/// entry in that header is what Caddy saw -- not attacker-controlled, unlike the header as a whole.
/// Falls back to peer_ip whenever peer_ip isn't trusted or the header is absent/empty.
[[nodiscard]] std::string resolve_client_ip(const std::string& peer_ip,
                                            const std::optional<std::string>& forwarded_for,
                                            const std::unordered_set<std::string>& trusted_proxies);

}  // namespace kfc::server
