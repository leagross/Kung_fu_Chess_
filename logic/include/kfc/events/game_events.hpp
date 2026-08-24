#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"

namespace kfc::events {

struct GameStarted {};

/// winner is nullopt for a draw.
struct GameEnded {
    std::optional<kfc::model::PieceColor> winner;
};

/// Networked play only: published each second during a dropped opponent's grace period.
struct OpponentCountdown {
    int seconds_remaining;
};

/// Networked play only: published when a dropped opponent reconnects in time.
struct OpponentReturned {};

}  // namespace kfc::events
