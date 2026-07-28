# `input/` — clicks into commands

Turning a pointer position into a game command. Knows nothing about a window,
an event loop or a graphics library: it is handed coordinates and asks the game
for a move.

| Header | Responsibility |
|--------|----------------|
| `board_mapper.hpp` | `BoardMapper` — pixel → cell, and back. Pure arithmetic over the fixed cell size. |
| `controller.hpp` | `Controller` — the two-click selection state machine: click a piece to select it, click again to move it there. Double-click jumps in place. |

## What `Controller` actually decides

It holds one piece of state — which cell is selected — and turns clicks into
`ClickOutcome`: `Selected`, `SelectionCleared`, `MoveRequested`, `JumpRequested`
or `Ignored`. It never decides legality; it asks an `IMoveRequester` and reports
what came back.

Two behaviours worth knowing:

- **Clicking another of your own pieces re-selects** rather than attempting a
  move onto it. Selecting a rook and then a knight means you changed your mind.
- **`controlled_color` restricts what can be picked up.** In networked play a
  client is given its own colour, so clicking an opponent's piece is ignored
  locally rather than sent and bounced back. That is a convenience only — the
  server re-checks ownership regardless, because a client cannot be trusted.
