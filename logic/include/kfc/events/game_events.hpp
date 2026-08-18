#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"

namespace kfc::events {

/// Published once, the moment a game becomes playable. Carries no data.
struct GameStarted {};

/// Published once, the moment a game is decided. winner is std::nullopt for a draw.
struct GameEnded {
    std::optional<kfc::model::PieceColor> winner;
};

/// Published (networked play only) each second while a dropped opponent's grace
/// period counts down. If the opponent never returns, a GameEnded follows.
struct OpponentCountdown {
    int seconds_remaining;
};

/// Published (networked play only) when a dropped opponent reconnects before
/// their grace period runs out, clearing the OpponentCountdown.
struct OpponentReturned {};

// Per-move "arrived/captured" events use kfc::model::ArrivalEvent directly on
// the bus; only whole-game start/end signals are defined here.

}  // namespace kfc::events
