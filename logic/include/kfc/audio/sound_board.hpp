#pragma once

#include "../../kfc/audio/sound.hpp"
#include "../../kfc/events/event_bus.hpp"

namespace kfc::audio {

/// Turns game events into sound cues. On construction it subscribes to a game's
/// EventBus and, for each event, asks an ISoundPlayer to play the matching
/// Sound: an ArrivalEvent that captured a piece -> Capture, otherwise Move;
/// GameStarted -> GameStart; GameEnded -> GameEnd. Pure mapping/wiring, headless
/// and testable with a recording player; the actual audio backend is injected.
///
/// Like the score panel and move log, this is just another bus subscriber -- it
/// holds no game state of its own, it only reacts.
class SoundBoard {
public:
    /// Subscribes to bus immediately. Both bus and player must outlive this
    /// SoundBoard (the subscriptions capture player by reference).
    SoundBoard(kfc::events::EventBus& bus, ISoundPlayer& player);
};

}  // namespace kfc::audio
