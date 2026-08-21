#pragma once

#include <filesystem>

#include "kfc/audio/sound.hpp"

namespace kfc::graphics::audio {

/// Windows-only ISoundPlayer: maps each Sound cue to a .wav file and plays
/// it asynchronously via PlaySound (winmm). Missing files stay silent rather
/// than crashing or beeping.
class WinSoundPlayer : public kfc::audio::ISoundPlayer {
public:
    explicit WinSoundPlayer(std::filesystem::path sounds_dir);
    void play(kfc::audio::Sound sound) override;

private:
    std::filesystem::path sounds_dir_;
};

}  // namespace kfc::graphics::audio
