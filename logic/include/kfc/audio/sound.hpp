#pragma once

namespace kfc::audio {

/// The distinct sound cues the game can ask for. Deliberately a small closed
/// set of *meanings*, not file names -- what a cue sounds like (or whether it
/// makes any noise at all) is entirely the ISoundPlayer's business.
enum class Sound {
    Move,       // a piece finished an ordinary move
    Capture,    // a move that took an enemy piece
    GameStart,  // the game just became playable
    GameEnd,    // the game was decided
};

/// Plays a sound cue. Abstracted so the event->sound mapping (SoundBoard) can
/// be unit-tested with a recording fake, while the real audio backend
/// (WinSoundPlayer, Windows-only, in the UI layer) stays out of the headless
/// core -- the same split kfc_core keeps from OpenCV. A player is free to no-op
/// (e.g. when a sound file is missing), which is exactly how the "mechanism now,
/// audio files later" stub behaves.
class ISoundPlayer {
public:
    virtual ~ISoundPlayer() = default;
    virtual void play(Sound sound) = 0;
};

}  // namespace kfc::audio
