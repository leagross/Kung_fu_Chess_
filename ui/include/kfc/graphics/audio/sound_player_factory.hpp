#pragma once

#include <filesystem>
#include <memory>

#include "kfc/audio/sound.hpp"

namespace kfc::graphics::audio {

/// The ISoundPlayer for the platform this was built for, reading its .wav files
/// from sounds_dir. Windows gets WinSoundPlayer (PlaySound, via winmm);
/// everywhere else gets a silent one until a cross-platform audio backend is
/// added -- see silent_sound_player.cpp.
///
/// Exists so the client never names a platform's player directly, exactly as
/// make_room_prompt() does for dialogs. Which one you get is decided in one
/// place, not at the call site.
[[nodiscard]] std::unique_ptr<kfc::audio::ISoundPlayer> make_sound_player(std::filesystem::path sounds_dir);

}  // namespace kfc::graphics::audio
