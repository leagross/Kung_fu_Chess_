# `logic/` — the game itself

The rules of Kung Fu Chess, and nothing else. No graphics, no sockets, no
database, no operating system. Everything here is plain portable C++20 and is
tested without any of those things being present.

This is the layer everything else is built on, and it depends on nothing.

## What is in here

| Folder | Responsibility | Guide |
|--------|----------------|-------|
| `model/` | `Board`, `Piece`, `Position`. A piece keeps a stable identity for its whole life, so animation and logs can follow it. | [README](include/kfc/model/README.md) |
| `rules/` | One rule class per piece kind behind `IPieceRule`, plus the `RuleEngine` that asks the right one. Illegal moves come back with a stable machine-readable reason. | [README](include/kfc/rules/README.md) |
| `realtime/` | What makes this *Kung Fu* chess: motion in transit, the arbiter that advances it, collisions, cooldowns, and the score/move-log/game-over observers. | [README](include/kfc/realtime/README.md) |
| `engine/` | `GameEngine` (accept or reject, then advance time) and `GameCore`, which assembles the whole stack in one place. | [README](include/kfc/engine/README.md) |
| `events/` | The type-based publish/subscribe bus and the whole-game signals. | [README](include/kfc/events/README.md) |
| `input/` | `Controller` (click a piece, click a destination) and `BoardMapper` (pixel → cell). Knows nothing about a window. | [README](include/kfc/input/README.md) |
| `io/` | Reading a board from a text layout, and printing one back. | [README](include/kfc/io/README.md) |
| `audio/` | `SoundBoard` — maps game events to sound *cues*. Which file a cue plays belongs to the UI layer. | [README](include/kfc/audio/README.md) |
| `texttests/` | `Game`, the headless local game, plus `IGameView` and the text-script driver. (The name is historic — this is production code.) | [README](include/kfc/texttests/README.md) |

## Two ideas worth knowing

**A move takes time.** `request_move` does not move the piece — it starts a
`Motion`. The piece occupies its *source* cell until it arrives, and the board
changes only at that instant. This is what makes head-on collisions, captures
in transit and simultaneous arrivals real situations rather than edge cases.

**Time is injected, never read.** Nothing here calls a clock. Callers pass the
elapsed milliseconds into `wait()`, which is why a test can play out a whole
game deterministically in microseconds while the real game passes it the actual
frame time.

## Tests

`tests/unit/` covers each piece rule, the rule engine, the arbiter, collisions,
promotion, cooldowns, the event bus and the observers.
`tests/integration/test_text_scripts.cpp` runs whole recorded games from text
fixtures and compares the final board.
