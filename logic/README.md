# `logic/` — the game itself

The rules of Kung Fu Chess, and nothing else. No graphics, no sockets, no
database, no operating system. Everything here is plain portable C++20 and is
tested without any of those things being present.

This is the layer everything else is built on, and it depends on nothing.

## What is in here

| Folder | Responsibility |
|--------|----------------|
| `model/` | `Board`, `Piece`, `Position`, `PieceColor`. A piece keeps a stable identity for its whole life, so animation and logs can follow it. |
| `rules/` | One rule class per piece kind — King, Queen, Rook, Bishop, Knight, Pawn, Drone — behind `IPieceRule`, plus the `RuleEngine` that asks the right one. Illegal moves come back with a stable machine-readable reason. |
| `realtime/` | What makes this *Kung Fu* chess: `Motion` (a piece in transit), `RealTimeArbiter` (advancing every motion by elapsed time), `CollisionResolver` (two pieces arriving at once), cooldown policies, and the observers for score, move log and game over. |
| `engine/` | `GameEngine` — accept or reject a move request, then advance time. `GameCore` assembles the whole stack in one place so local play and the server build it identically. |
| `events/` | A small type-based publish/subscribe bus. Subscribe to a type, publish a value of it. Used for arrivals, game start/end and the disconnect countdown. |
| `input/` | `Controller` (click a piece, click a destination) and `BoardMapper` (pixel → cell). Knows nothing about a window; it is handed coordinates. |
| `io/` | Reading a board from a text layout, and printing one back. |
| `audio/` | `SoundBoard` — maps game events to sound *cues*. Which file a cue plays, or whether it makes any noise, belongs to the UI layer. |
| `texttests/` | `Game`, the headless local game, plus a command processor that drives it from text scripts. |

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
