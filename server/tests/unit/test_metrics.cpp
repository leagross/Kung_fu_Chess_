#include "kfc/server/metrics.hpp"

#include <gtest/gtest.h>

#include <chrono>
#include <string>

using kfc::server::Metrics;

TEST(MetricsTest, StartsAtZeroForEveryCounter) {
    Metrics metrics;

    std::string rendered = metrics.render(/*active_connections=*/0, /*active_rooms=*/0, /*worker_threads=*/0);

    EXPECT_NE(rendered.find("kfc_active_connections 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_active_rooms 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_worker_threads 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_messages_received_total 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_messages_rejected_total 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_messages_undecodable_total 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_moves_processed_total 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_tick_duration_seconds_sum 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_tick_duration_seconds_count 0"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_tick_duration_seconds_max 0"), std::string::npos);
}

TEST(MetricsTest, RenderReflectsWhateverGaugeValuesAreHandedInEachCall) {
    Metrics metrics;

    std::string rendered = metrics.render(/*active_connections=*/7, /*active_rooms=*/3, /*worker_threads=*/4);

    EXPECT_NE(rendered.find("kfc_active_connections 7"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_active_rooms 3"), std::string::npos);
    EXPECT_NE(rendered.find("kfc_worker_threads 4"), std::string::npos);
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

TEST(MetricsTest, RecordTickAccumulatesSumAndCountAndTracksTheMax) {
    Metrics metrics;

    metrics.record_tick(std::chrono::milliseconds(2));
    metrics.record_tick(std::chrono::milliseconds(5));
    metrics.record_tick(std::chrono::milliseconds(1));

    EXPECT_EQ(metrics.tick_count(), 3u);
    EXPECT_EQ(metrics.tick_total_ns(), std::chrono::nanoseconds(std::chrono::milliseconds(8)).count());
    EXPECT_EQ(metrics.tick_max_ns(), std::chrono::nanoseconds(std::chrono::milliseconds(5)).count());
}

TEST(MetricsTest, RenderIsPrometheusTextExpositionFormatWithHelpAndTypeLines) {
    Metrics metrics;

    std::string rendered = metrics.render(0, 0, 0);

    EXPECT_NE(rendered.find("# HELP kfc_active_connections"), std::string::npos);
    EXPECT_NE(rendered.find("# TYPE kfc_active_connections gauge"), std::string::npos);
    EXPECT_NE(rendered.find("# HELP kfc_messages_received_total"), std::string::npos);
    EXPECT_NE(rendered.find("# TYPE kfc_messages_received_total counter"), std::string::npos);
    EXPECT_NE(rendered.find("# TYPE kfc_tick_duration_seconds_sum counter"), std::string::npos);
    EXPECT_NE(rendered.find("# TYPE kfc_tick_duration_seconds_max gauge"), std::string::npos);
}
