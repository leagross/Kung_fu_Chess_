#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "kfc/protocol/gameplay_config.hpp"

using namespace kfc::protocol;
using kfc::model::PieceKind;

namespace {

// Writes text to a unique temp file and returns its path; removed by the
// caller (or left for the OS -- it's a temp dir).
std::string write_temp(const std::string& name, const std::string& text) {
    std::filesystem::path path = std::filesystem::temp_directory_path() / name;
    std::ofstream(path) << text;
    return path.string();
}

}  // namespace

TEST(GameplayConfigTest, DefaultsReproduceTheOldHardcodedValues) {
    GameplayConfig config;

    EXPECT_DOUBLE_EQ(config.meters_per_cell, 0.6);
    EXPECT_DOUBLE_EQ(config.speed_for(PieceKind::Queen), 1.5);
    EXPECT_DOUBLE_EQ(config.speed_for(PieceKind::Drone), 1.5);  // no override by default
    EXPECT_EQ(config.standard_cooldown_ms, 2500);
    EXPECT_EQ(config.jump_cooldown_ms, 834);
    EXPECT_EQ(config.value_for(PieceKind::Queen), 0);  // no values by default
}

TEST(GameplayConfigTest, SpeedForUsesTheOverrideWhenPresentOtherwiseTheDefault) {
    GameplayConfig config;
    config.default_speed_m_per_sec = 2.0;
    config.speed_overrides[PieceKind::Drone] = 1.0;

    EXPECT_DOUBLE_EQ(config.speed_for(PieceKind::Rook), 2.0);
    EXPECT_DOUBLE_EQ(config.speed_for(PieceKind::Drone), 1.0);
}

TEST(GameplayConfigTest, LoadsEveryFieldFromAFullFile) {
    std::string path = write_temp("kfc_gameplay_full.json", R"({
        "meters_per_cell": 0.5,
        "speed_m_per_sec": { "default": 1.35, "Drone": 1.0 },
        "cooldown_ms": { "standard": 3000, "jump": 700 },
        "piece_value": { "Pawn": 1, "Queen": 9, "Drone": 2 }
    })");

    GameplayConfig config = load_gameplay_config(path);
    std::filesystem::remove(path);

    EXPECT_DOUBLE_EQ(config.meters_per_cell, 0.5);
    EXPECT_DOUBLE_EQ(config.speed_for(PieceKind::Bishop), 1.35);
    EXPECT_DOUBLE_EQ(config.speed_for(PieceKind::Drone), 1.0);
    EXPECT_EQ(config.standard_cooldown_ms, 3000);
    EXPECT_EQ(config.jump_cooldown_ms, 700);
    EXPECT_EQ(config.value_for(PieceKind::Queen), 9);
    EXPECT_EQ(config.value_for(PieceKind::Drone), 2);
}

TEST(GameplayConfigTest, APartialFileKeepsDefaultsForOmittedFields) {
    std::string path = write_temp("kfc_gameplay_partial.json", R"({
        "speed_m_per_sec": { "default": 2.0 }
    })");

    GameplayConfig config = load_gameplay_config(path);
    std::filesystem::remove(path);

    EXPECT_DOUBLE_EQ(config.speed_for(PieceKind::Rook), 2.0);
    EXPECT_DOUBLE_EQ(config.meters_per_cell, 0.6);   // default kept
    EXPECT_EQ(config.standard_cooldown_ms, 2500);     // default kept
}

TEST(GameplayConfigTest, ThrowsOnAMissingFile) {
    EXPECT_THROW(load_gameplay_config("no_such_gameplay_file.json"), std::runtime_error);
}

TEST(GameplayConfigTest, ThrowsOnAnUnknownPieceKind) {
    std::string path = write_temp("kfc_gameplay_badkind.json", R"({
        "piece_value": { "Wizard": 5 }
    })");

    EXPECT_THROW(load_gameplay_config(path), std::runtime_error);
    std::filesystem::remove(path);
}

TEST(GameplayConfigTest, ProvidersDelegateToTheConfig) {
    GameplayConfig config;
    config.default_speed_m_per_sec = 1.7;
    config.speed_overrides[PieceKind::Drone] = 1.0;
    config.piece_values[PieceKind::Rook] = 5;

    GameplaySpeedProvider speed(config);
    GameplayCooldownPolicy cooldown(1234);
    GameplayValueProvider value(config);

    EXPECT_DOUBLE_EQ(speed.speed_m_per_sec(PieceKind::Bishop), 1.7);
    EXPECT_DOUBLE_EQ(speed.speed_m_per_sec(PieceKind::Drone), 1.0);
    EXPECT_EQ(cooldown.cooldown_ms(), 1234);
    EXPECT_EQ(value.value_of(PieceKind::Rook), 5);
    EXPECT_EQ(value.value_of(PieceKind::Pawn), 0);
}
