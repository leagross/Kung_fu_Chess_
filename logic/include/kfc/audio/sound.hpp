#pragma once

namespace kfc::audio {

/// A closed set of sound cue *meanings*, not file names -- what a cue sounds
/// like is entirely the ISoundPlayer's business.
enum class Sound {
    Move,
    Capture,
    GameStart,
    GameEnd,
};

/// Abstracted so the event->sound mapping can be unit-tested with a fake,
/// keeping the real audio backend out of the headless core.
class ISoundPlayer {
public:
    virtual ~ISoundPlayer() = default;
    virtual void play(Sound sound) = 0;
};

}  // namespace kfc::audio
