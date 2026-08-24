#pragma once

namespace kfc::audio {

/// Cue meanings, not file names -- what a cue sounds like is ISoundPlayer's business.
enum class Sound {
    Move,
    Capture,
    GameStart,
    GameEnd,
};

/// Abstracted so event->sound mapping can be unit-tested without the real audio backend.
class ISoundPlayer {
public:
    virtual ~ISoundPlayer() = default;
    virtual void play(Sound sound) = 0;
};

}  // namespace kfc::audio
