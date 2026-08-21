#pragma once

#include <string>
#include <unordered_map>

#include "kfc/model/piece.hpp"
#include "kfc/realtime/cooldown_policy.hpp"
#include "kfc/realtime/piece_speed_provider.hpp"
#include "kfc/realtime/piece_value_provider.hpp"

namespace kfc::protocol {

/// Single source of truth for gameplay timing/values, read from gameplay.json
/// so the server and local client agree on piece speed/cooldowns/values.
struct GameplayConfig {
    /// Converts a piece's speed (m/s) into a per-cell-step duration.
    double meters_per_cell = 0.6;

    /// Speed for any kind without an explicit override below.
    double default_speed_m_per_sec = 1.5;
    /// Per-kind speed overrides (e.g. the Drone at 1.0).
    std::unordered_map<kfc::model::PieceKind, double> speed_overrides;

    /// Rest after an ordinary move, and after a jump-in-place, respectively.
    int standard_cooldown_ms = 2500;
    int jump_cooldown_ms = 834;

    /// Material value credited for capturing each kind; a kind absent from
    /// the map is worth 0.
    std::unordered_map<kfc::model::PieceKind, int> piece_values;

    /// Speed for kind: its override if present, otherwise default_speed.
    [[nodiscard]] double speed_for(kfc::model::PieceKind kind) const;
    /// Capture value for kind, or 0 if not listed.
    [[nodiscard]] int value_for(kfc::model::PieceKind kind) const;
};

/// Parses a GameplayConfig from the JSON file at path. Throws
/// std::runtime_error if the file can't be opened or is malformed. Fields
/// left out of the file keep their GameplayConfig default.
[[nodiscard]] GameplayConfig load_gameplay_config(const std::string& path);

// Provider adapters: expose a GameplayConfig through the backend's own
// strategy interfaces. Each holds a reference to the config, which must
// outlive it.

/// IPieceSpeedProvider backed by a GameplayConfig.
class GameplaySpeedProvider : public kfc::model::IPieceSpeedProvider {
public:
    explicit GameplaySpeedProvider(const GameplayConfig& config) : config_(config) {}
    [[nodiscard]] double speed_m_per_sec(kfc::model::PieceKind kind) const override;

private:
    const GameplayConfig& config_;
};

/// ICooldownPolicy returning a single fixed value (the standard or jump
/// cooldown pulled from a GameplayConfig by the caller).
class GameplayCooldownPolicy : public kfc::model::ICooldownPolicy {
public:
    explicit GameplayCooldownPolicy(int cooldown_ms) : cooldown_ms_(cooldown_ms) {}
    [[nodiscard]] int cooldown_ms() const override;

private:
    int cooldown_ms_;
};

/// IPieceValueProvider backed by a GameplayConfig.
class GameplayValueProvider : public kfc::model::IPieceValueProvider {
public:
    explicit GameplayValueProvider(const GameplayConfig& config) : config_(config) {}
    [[nodiscard]] int value_of(kfc::model::PieceKind kind) const override;

private:
    const GameplayConfig& config_;
};

}  // namespace kfc::protocol
