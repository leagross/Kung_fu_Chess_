#include "kfc/server/client_ip.hpp"

#include <string_view>

namespace kfc::server {

namespace {

std::string_view trim(std::string_view text) {
    std::size_t start = text.find_first_not_of(' ');
    if (start == std::string_view::npos) {
        return {};
    }
    std::size_t end = text.find_last_not_of(' ');
    return text.substr(start, end - start + 1);
}

}  // namespace

std::string resolve_client_ip(const std::string& peer_ip, const std::optional<std::string>& forwarded_for,
                              const std::unordered_set<std::string>& trusted_proxies) {
    if (!forwarded_for.has_value() || trusted_proxies.count(peer_ip) == 0) {
        return peer_ip;
    }
    std::string_view xff = *forwarded_for;
    std::size_t last_comma = xff.find_last_of(',');
    std::string_view last_hop = trim(last_comma == std::string_view::npos ? xff : xff.substr(last_comma + 1));
    return last_hop.empty() ? peer_ip : std::string(last_hop);
}

}  // namespace kfc::server
