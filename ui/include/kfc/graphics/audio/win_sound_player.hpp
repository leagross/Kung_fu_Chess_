#pragma once

#include <filesystem>

#include "kfc/audio/sound.hpp"

namespace kfc::graphics::audio {

/// The real, Windows-only ISoundPlayer: maps each Sound cue to a .wav file in a
/// sounds directory and plays it asynchronously via the OS (PlaySound from
/// winmm) -- no audio library dependency, the same "let Windows do it" approach
/// the CTD SERVER lecture takes for its MessageBox/pop-up.
///
/// Deliberately tolerant of missing files: if a cue's .wav isn't present it
/// simply stays silent (no crash, no default beep). That is exactly the "wire
/// the whole mechanism now, drop in real audio files later" stub -- the game
/// plays correctly today and gains sound the moment the .wav files appear in
/// the sounds directory (see its README for the expected file names).
class WinSoundPlayer : public kfc::audio::ISoundPlayer {
public:
    explicit WinSoundPlayer(std::filesystem::path sounds_dir);
    void play(kfc::audio::Sound sound) override;

private:
    std::filesystem::path sounds_dir_;
};

}  // namespace kfc::graphics::audio
