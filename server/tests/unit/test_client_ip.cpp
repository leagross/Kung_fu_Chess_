#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <unordered_set>

#include "kfc/server/client_ip.hpp"

using kfc::server::resolve_client_ip;

TEST(ClientIpTest, NoHeaderMeansTheTcpPeerIsTheClient) {
    EXPECT_EQ(resolve_client_ip("1.2.3.4", std::nullopt, {"1.2.3.4"}), "1.2.3.4");
}

TEST(ClientIpTest, AnUntrustedPeersForwardedForHeaderIsIgnored) {
    // The peer here is not in trusted_proxies -- e.g. a client talking to kfc_server directly,
    // bypassing Caddy, with a hand-crafted X-Forwarded-For meant to spoof someone else's identity.
    EXPECT_EQ(resolve_client_ip("9.9.9.9", std::optional<std::string>("1.2.3.4"), {"10.0.0.1"}), "9.9.9.9");
}

TEST(ClientIpTest, ATrustedProxysForwardedForSuppliesTheRealClient) {
    EXPECT_EQ(resolve_client_ip("10.0.0.1", std::optional<std::string>("1.2.3.4"), {"10.0.0.1"}), "1.2.3.4");
}

TEST(ClientIpTest, MultipleHopsUseTheLastOneTheTrustedProxyItselfAppended) {
    // Everything left of the last comma could have been supplied by the client itself; only the
    // last entry is what the trusted proxy saw on its own socket.
    EXPECT_EQ(resolve_client_ip("10.0.0.1", std::optional<std::string>("6.6.6.6, 1.2.3.4"), {"10.0.0.1"}),
             "1.2.3.4");
}

TEST(ClientIpTest, AnEmptyForwardedForHeaderFallsBackToThePeer) {
    EXPECT_EQ(resolve_client_ip("10.0.0.1", std::optional<std::string>(""), {"10.0.0.1"}), "10.0.0.1");
}
