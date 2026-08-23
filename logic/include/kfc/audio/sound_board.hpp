#pragma once

#include "../../kfc/audio/sound.hpp"
#include "../../kfc/events/event_bus.hpp"

namespace kfc::audio {

/// Maps game events to sound cues via an injected ISoundPlayer; holds no game state.
class SoundBoard {
public:
    /// Both bus and player must outlive this SoundBoard.
    SoundBoard(kfc::events::EventBus& bus, ISoundPlayer& player);
};

}  // namespace kfc::audio
