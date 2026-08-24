#pragma once

#include <filesystem>
#include <memory>

#include "kfc/audio/sound.hpp"

namespace kfc::graphics::audio {

/// The ISoundPlayer for the platform this was built for, reading .wav files from sounds_dir.
[[nodiscard]] std::unique_ptr<kfc::audio::ISoundPlayer> make_sound_player(std::filesystem::path sounds_dir);

}  // namespace kfc::graphics::audio
