# `audio/` — events into sound cues

The mapping from what happened to what should be heard. Headless on purpose:
there is no audio library here, so this is testable with a recording fake and
`logic/` stays free of any platform dependency.

| Header | Responsibility |
|--------|----------------|
| `sound.hpp` | `Sound` — the closed set of cues: `Move`, `Capture`, `GameStart`, `GameEnd`. Plus `ISoundPlayer`, which plays one. |
| `sound_board.hpp` | `SoundBoard` — subscribes to the event bus and asks the player for the matching cue. |

## Meanings, not filenames

A `Sound` is a *cue*, not a file. Which `.wav` it plays — or whether it makes
any noise at all — is entirely the `ISoundPlayer`'s business, and that is what
lets the same mapping drive `PlaySound` on Windows and miniaudio elsewhere
without `SoundBoard` changing. A player is free to no-op, which is exactly how
the whole system behaved before any audio files existed.

The four filenames each cue looks for are listed in
[`assets/README.md`](../../../../assets/README.md).

## Adding a cue

Add an enumerator to `Sound`, a case in `SoundBoard`, and a filename in each
player. There is deliberately no cue for an illegal move or for the disconnect
countdown — adding one is this three-step change, not a configuration tweak.
