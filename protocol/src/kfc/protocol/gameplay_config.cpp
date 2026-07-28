#include "kfc/protocol/gameplay_config.hpp"

#include <fstream>
#include <optional>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace kfc::protocol {

namespace {

using kfc::model::PieceKind;

std::optional<PieceKind> piece_kind_from_string(const std::string& text) {
    if (text == "King") return PieceKind::King;
    if (text == "Queen") return PieceKind::Queen;
    if (text == "Rook") return PieceKind::Rook;
    if (text == "Bishop") return PieceKind::Bishop;
    if (text == "Knight") return PieceKind::Knight;
    if (text == "Pawn") return PieceKind::Pawn;
    if (text == "Drone") return PieceKind::Drone;
    return std::nullopt;
}

}  // namespace

double GameplayConfig::speed_for(PieceKind kind) const {
    auto it = speed_overrides.find(kind);
    return it != speed_overrides.end() ? it->second : default_speed_m_per_sec;
}

int GameplayConfig::value_for(PieceKind kind) const {
    auto it = piece_values.find(kind);
    return it != piece_values.end() ? it->second : 0;
}

GameplayConfig load_gameplay_config(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open gameplay config: " + path);
    }

    nlohmann::json json;
    try {
        file >> json;
    } catch (const std::exception& e) {
        throw std::runtime_error("Malformed gameplay config " + path + ": " + e.what());
    }

    // Every field is optional: anything absent keeps the struct default, so a
    // partial file is valid and behaves like the old hardcoded values.
    GameplayConfig config;

    if (json.contains("meters_per_cell")) {
        config.meters_per_cell = json.at("meters_per_cell").get<double>();
    }

    if (json.contains("speed_m_per_sec")) {
        const nlohmann::json& speeds = json.at("speed_m_per_sec");
        if (speeds.contains("default")) {
            config.default_speed_m_per_sec = speeds.at("default").get<double>();
        }
        for (auto it = speeds.begin(); it != speeds.end(); ++it) {
            if (it.key() == "default") {
                continue;
            }
            std::optional<PieceKind> kind = piece_kind_from_string(it.key());
            if (!kind.has_value()) {
                throw std::runtime_error("Unknown piece kind '" + it.key() + "' in gameplay config " + path);
            }
            config.speed_overrides[*kind] = it.value().get<double>();
        }
    }

    if (json.contains("cooldown_ms")) {
        const nlohmann::json& cooldowns = json.at("cooldown_ms");
        if (cooldowns.contains("standard")) {
            config.standard_cooldown_ms = cooldowns.at("standard").get<int>();
        }
        if (cooldowns.contains("jump")) {
            config.jump_cooldown_ms = cooldowns.at("jump").get<int>();
        }
    }

    if (json.contains("piece_value")) {
        const nlohmann::json& values = json.at("piece_value");
        for (auto it = values.begin(); it != values.end(); ++it) {
            std::optional<PieceKind> kind = piece_kind_from_string(it.key());
            if (!kind.has_value()) {
                throw std::runtime_error("Unknown piece kind '" + it.key() + "' in gameplay config " + path);
            }
            config.piece_values[*kind] = it.value().get<int>();
        }
    }

    return config;
}

double GameplaySpeedProvider::speed_m_per_sec(PieceKind kind) const {
    return config_.speed_for(kind);
}

int GameplayCooldownPolicy::cooldown_ms() const {
    return cooldown_ms_;
}

int GameplayValueProvider::value_of(PieceKind kind) const {
    return config_.value_for(kind);
}

}  // namespace kfc::protocol
