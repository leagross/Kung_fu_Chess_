// The ISoundPlayer for every platform except Windows, which has PlaySound in
// the OS itself (see win_sound_player.cpp). Compiled only off Windows.
//
// miniaudio is a single public-domain header that opens whatever audio API the
// machine actually has -- ALSA or PulseAudio on Linux, CoreAudio on macOS -- by
// dlopen at runtime. So this needs no audio -dev package to build against and
// no configuration to pick a backend; it just plays.
//
// The behaviour matches WinSoundPlayer's exactly, which is what keeps the game
// identical across platforms:
//   * playback is asynchronous -- play() returns immediately and never stalls
//     the render loop;
//   * a missing .wav is silence, not an error and not a crash. Cues whose files
//     have not been added yet simply make no noise, which is how the sound
//     system behaved before any files existed at all.

#include <filesystem>
#include <string>
#include <utility>

#include "kfc/graphics/audio/sound_player_factory.hpp"

// Exactly one translation unit may define this, and this is it.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace kfc::graphics::audio {

namespace {

/// Which .wav each cue plays. The same four names WinSoundPlayer uses -- see
/// assets/README.md. Lowercase on purpose: filenames are case-sensitive on
/// Linux and macOS, unlike Windows.
const char* file_name_for(kfc::audio::Sound sound) {
    switch (sound) {
        case kfc::audio::Sound::Move: return "move.wav";
        case kfc::audio::Sound::Capture: return "capture.wav";
        case kfc::audio::Sound::GameStart: return "start.wav";
        case kfc::audio::Sound::GameEnd: return "end.wav";
    }
    return "";
}

class MiniaudioSoundPlayer : public kfc::audio::ISoundPlayer {
public:
    explicit MiniaudioSoundPlayer(std::filesystem::path sounds_dir) : sounds_dir_(std::move(sounds_dir)) {
        // One engine for the process, opened once. If the machine has no usable
        // audio device -- a headless server, a container, a CI runner -- this
        // fails, and the player then behaves exactly like the silent one rather
        // than taking the game down with it.
        ready_ = ma_engine_init(nullptr, &engine_) == MA_SUCCESS;
    }

    ~MiniaudioSoundPlayer() override {
        if (ready_) {
            ma_engine_uninit(&engine_);
        }
    }

    MiniaudioSoundPlayer(const MiniaudioSoundPlayer&) = delete;
    MiniaudioSoundPlayer& operator=(const MiniaudioSoundPlayer&) = delete;

    void play(kfc::audio::Sound sound) override {
        if (!ready_) {
            return;
        }
        std::string path = (sounds_dir_ / file_name_for(sound)).string();
        // Fire and forget: miniaudio decodes and mixes on its own thread. The
        // return value is deliberately ignored -- the only way it fails here is
        // a file that isn't there, which is silence by design.
        (void)ma_engine_play_sound(&engine_, path.c_str(), nullptr);
    }

private:
    std::filesystem::path sounds_dir_;
    ma_engine engine_{};
    bool ready_ = false;
};

}  // namespace

std::unique_ptr<kfc::audio::ISoundPlayer> make_sound_player(std::filesystem::path sounds_dir) {
    return std::make_unique<MiniaudioSoundPlayer>(std::move(sounds_dir));
}

}  // namespace kfc::graphics::audio
