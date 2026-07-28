# `texttests/` — the local game, and the view every client sees through

Despite the name, this is production code, not test code. `Game` is the real
single-player game and `IGameView` is the interface both clients render
through. The name is historic — it started as the harness for the text-script
tests — and is misleading enough to be worth renaming (`localgame/` would say
what it is).

| Header | Responsibility |
|--------|----------------|
| `game_view.hpp` | `IGameView` — exactly what a renderer needs from "the current game": the board, a click, a jump, `wait`, and the event bus. |
| `game.hpp` | `Game` — local single-player. Owns a `GameCore` and a `Controller`, and simulates in-process. |
| `input_reader.hpp` | Reads a text script: a board layout plus a list of commands. |
| `command_processor.hpp` | Applies those commands to a `Game`, which is how a whole recorded game is replayed in a test. |

## Why `IGameView` is the important type here

It is the seam that makes local and networked play interchangeable. `Game`
implements it by simulating locally; `ServerLink` (in `ui/`) implements it by
mirroring an authoritative server. The renderer, the animator and the HUD are
written once and never learn which one they were handed.

The difference is only in where the truth lives. Locally the board *is* the
simulation. Over the network the board is a mirror that changes only when the
server says an arrival happened — the client predicts motion for smoothness, but
a prediction never moves a piece.
