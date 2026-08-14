#include "kfc/server/metrics.hpp"

#include <gtest/gtest.h>

#include <string>

using kfc::server::Metrics;

TEST(MetricsTest, StartsAtZeroForEveryCounter) {
    Metrics metrics;

    std::string rendered = metrics.render(/*active_connections=*/0, /*active_rooms=*/0);

    EXPECT_NE(rendered.find("kfc_active_connections 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_active_rooms 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_messages_received_total 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_messages_rejected_total 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_messages_undecodable_total 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_moves_processed_total 0"), std::string::npos);
}

TEST(MetricsTest, RenderReflectsWhateverGaugeValuesAreHandedInEachCall) {
    Metrics metrics;

    std::string rendered = metrics.render(/*active_connections=*/7, /*active_rooms=*/3);

    EXPECT_NE(rendered.find("kfc_active_connections 7"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_active_rooms 3"), std::string::npos);
}

TEST(MetricsTest, EachCounterTracksOnlyItsOwnEvent) {
    Metrics metrics;

    metrics.message_received();
    metrics.message_received();
    metrics.message_rejected();
    metrics.message_undecodable();
    metrics.move_processed();
    metrics.move_processed();
    metrics.move_processed();

    EXPECT_EQ(metrics.messages_received(), 2u);
    EXPECT_EQ(metrics.messages_rejected(), 1u);
    EXPECT_EQ(metrics.messages_undecodable(), 1u);
    EXPECT_EQ(metrics.moves_processed(), 3u);
}

TEST(MetricsTest, RenderIsPrometheusTextExpositionFormatWithHelpAndTypeLines) {
    Metrics metrics;

    std::string rendered = metrics.render(0, 0);

    EXPECT_NE(rendered.find("# HELP kfc_active_connections"), std::string::npos);
    EXPECT_NE(rendered.find("# TYPE kfc_active_connections gauge"), std::string::npos);
    EXPECT_NE(rendered.find("# HELP kfc_messages_received_total"), std::string::npos);
    EXPECT_NE(rendered.find("# TYPE kfc_messages_received_total counter"), std::string::npos);
}
