#pragma once

#include <optional>

#include "../../kfc/model/piece.hpp"

namespace kfc::events {

/// Published once, the moment a game becomes playable. Carries no data -- it is
/// purely a "now" signal, for a start-of-game animation and its sound cue.
struct GameStarted {};

/// Published once, the moment a game is decided. winner is std::nullopt for a
/// draw (mirrors GameOverObserver). Drives the end-of-game animation/banner and
/// its sound cue.
struct GameEnded {
    std::optional<kfc::model::PieceColor> winner;
};

/// Published (networked play only) each second while a dropped opponent's grace
/// period counts down -- seconds_remaining goes N..1 -- so the UI can show the
/// countdown. Mirrors the protocol's OpponentDisconnected; if the opponent
/// never returns a GameEnded follows.
struct OpponentCountdown {
    int seconds_remaining;
};

/// Published (networked play only) when a dropped opponent came back before
/// their grace ran out: the countdown OpponentCountdown started must be cleared
/// and play resumes. Mirrors the protocol's OpponentReconnected. The other way a
/// countdown ends is GameEnded (they never returned).
struct OpponentReturned {};

// The per-move "a piece arrived / captured" event is kfc::model::ArrivalEvent
// itself, published on the bus as-is -- the score panel, move log, and
// move/capture sounds subscribe to that type directly rather than to a wrapper
// (see EventBus). Only the whole-game start/end signals, which have no existing
// type, are defined here.

}  // namespace kfc::events
