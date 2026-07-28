#include "kfc/protocol/gameplay_config.hpp"

#include "kfc/model/piece_names.hpp"

#include <fstream>
#include <optional>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace kfc::protocol {

namespace {

using kfc::model::PieceKind;

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
    // partial file is valid and behaves like the old hardcoded values. Each
    // one is looked up once, with find rather than contains-then-at: the pair
    // searched the object twice for every field that was actually present,
    // which is the common case.
    GameplayConfig config;

    if (auto meters = json.find("meters_per_cell"); meters != json.end()) {
        config.meters_per_cell = meters->get<double>();
    }

    if (auto speeds = json.find("speed_m_per_sec"); speeds != json.end()) {
        for (auto it = speeds->begin(); it != speeds->end(); ++it) {
            if (it.key() == "default") {
                config.default_speed_m_per_sec = it.value().get<double>();
                continue;
            }
            std::optional<PieceKind> kind = kfc::model::piece_kind_from_name(it.key());
            if (!kind.has_value()) {
                throw std::runtime_error("Unknown piece kind '" + it.key() + "' in gameplay config " + path);
            }
            config.speed_overrides[*kind] = it.value().get<double>();
        }
    }

    if (auto cooldowns = json.find("cooldown_ms"); cooldowns != json.end()) {
        if (auto standard = cooldowns->find("standard"); standard != cooldowns->end()) {
            config.standard_cooldown_ms = standard->get<int>();
        }
        if (auto jump = cooldowns->find("jump"); jump != cooldowns->end()) {
            config.jump_cooldown_ms = jump->get<int>();
        }
    }

    if (auto values = json.find("piece_value"); values != json.end()) {
        for (auto it = values->begin(); it != values->end(); ++it) {
            std::optional<PieceKind> kind = kfc::model::piece_kind_from_name(it.key());
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
