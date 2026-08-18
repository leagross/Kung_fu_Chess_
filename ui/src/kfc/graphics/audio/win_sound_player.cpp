#include "kfc/graphics/audio/win_sound_player.hpp"

#include <utility>

#include "kfc/graphics/audio/sound_player_factory.hpp"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
// <mmsystem.h> (PlaySound) must come after <windows.h>.
#include <mmsystem.h>

namespace kfc::graphics::audio {

namespace {

const wchar_t* file_name_for(kfc::audio::Sound sound) {
    switch (sound) {
        case kfc::audio::Sound::Move: return L"move.wav";
        case kfc::audio::Sound::Capture: return L"capture.wav";
        case kfc::audio::Sound::GameStart: return L"start.wav";
        case kfc::audio::Sound::GameEnd: return L"end.wav";
    }
    return L"";
}

}  // namespace

WinSoundPlayer::WinSoundPlayer(std::filesystem::path sounds_dir) : sounds_dir_(std::move(sounds_dir)) {}

void WinSoundPlayer::play(kfc::audio::Sound sound) {
    std::filesystem::path path = sounds_dir_ / file_name_for(sound);
    // SND_ASYNC: don't stall the render loop. SND_NODEFAULT: stay silent
    // rather than beep if the .wav is absent.
    PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT);
}

std::unique_ptr<kfc::audio::ISoundPlayer> make_sound_player(std::filesystem::path sounds_dir) {
    return std::make_unique<WinSoundPlayer>(std::move(sounds_dir));
}

}  // namespace kfc::graphics::audio
