// The non-Windows ISoundPlayer. Compiled only off Windows, where WinSoundPlayer
// (PlaySound, winmm) does not exist.
//
// It is deliberately silent rather than absent. ISoundPlayer's contract already
// allows a player to no-op -- that is exactly how the whole sound system behaved
// before any .wav files existed -- so the game runs identically on Linux and
// macOS, just without cues. Everything else about audio (the event->cue mapping
// in kfc::audio::SoundBoard, the bus wiring, the .wav files on disk) is portable
// and untouched.
//
// To give these platforms real audio, replace play() below with a
// cross-platform backend -- miniaudio (a single public-domain header) is the
// smallest option -- and nothing outside this file has to change.

#include <filesystem>
#include <utility>

#include "kfc/graphics/audio/sound_player_factory.hpp"

namespace kfc::graphics::audio {

namespace {

class SilentSoundPlayer : public kfc::audio::ISoundPlayer {
public:
    explicit SilentSoundPlayer(std::filesystem::path sounds_dir) : sounds_dir_(std::move(sounds_dir)) {}

    void play(kfc::audio::Sound /*sound*/) override {
        // Intentionally nothing -- see this file's own comment.
    }

private:
    // Unused today, kept so a real backend added here has the directory it
    // needs without changing the factory's signature.
    std::filesystem::path sounds_dir_;
};

}  // namespace

std::unique_ptr<kfc::audio::ISoundPlayer> make_sound_player(std::filesystem::path sounds_dir) {
    return std::make_unique<SilentSoundPlayer>(std::move(sounds_dir));
}

}  // namespace kfc::graphics::audio
