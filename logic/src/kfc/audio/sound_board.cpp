#include "../../../include/kfc/audio/sound_board.hpp"

#include "../../../include/kfc/events/game_events.hpp"
#include "../../../include/kfc/realtime/arrival_event.hpp"

namespace kfc::audio {

SoundBoard::SoundBoard(kfc::events::EventBus& bus, ISoundPlayer& player) {
    bus.subscribe<kfc::model::ArrivalEvent>([&player](const kfc::model::ArrivalEvent& event) {
        player.play(event.captured_piece.has_value() ? Sound::Capture : Sound::Move);
    });
    bus.subscribe<kfc::events::GameStarted>([&player](const kfc::events::GameStarted&) {
        player.play(Sound::GameStart);
    });
    bus.subscribe<kfc::events::GameEnded>([&player](const kfc::events::GameEnded&) {
        player.play(Sound::GameEnd);
    });
}

}  // namespace kfc::audio
