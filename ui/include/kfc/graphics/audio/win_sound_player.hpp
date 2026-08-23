#pragma once

#include <filesystem>

#include "kfc/audio/sound.hpp"

namespace kfc::graphics::audio {

/// Plays each Sound cue asynchronously via PlaySound (winmm); missing files stay silent.
class WinSoundPlayer : public kfc::audio::ISoundPlayer {
public:
    explicit WinSoundPlayer(std::filesystem::path sounds_dir);
    void play(kfc::audio::Sound sound) override;

private:
    std::filesystem::path sounds_dir_;
};

}  // namespace kfc::graphics::audio
