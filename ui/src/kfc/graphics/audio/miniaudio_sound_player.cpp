// The ISoundPlayer for every platform except Windows (see win_sound_player.cpp).
// Compiled only off Windows. miniaudio opens whatever audio API the machine
// has (ALSA/PulseAudio/CoreAudio) via dlopen at runtime, needing no -dev
// package to build against.

#include <filesystem>
#include <string>
#include <utility>

#include "kfc/graphics/audio/sound_player_factory.hpp"

// Exactly one translation unit may define this, and this is it.
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

namespace kfc::graphics::audio {

namespace {

// Lowercase on purpose: filenames are case-sensitive on Linux/macOS.
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
        // No usable audio device (headless/CI) fails here; play() then stays silent.
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
        // Fire and forget; a missing file just means no sound.
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
