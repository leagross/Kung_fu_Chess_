# `model/` — the state of a game

The nouns. What a board is, what a piece is, where things are. No rules, no
time, no rendering — a `Board` cannot tell you whether a move is legal.

| Header | What it defines |
|--------|-----------------|
| `position.hpp` | `Position{row, col}`. A plain value with equality, so it can be compared, stored in maps and printed. Row 0 is the top of the board as printed. |
| `piece.hpp` | `Piece` (id, colour, kind, cell, state, has_moved), plus the `PieceId`, `PieceColor`, `PieceKind` and `PieceState` types. |
| `piece_names.hpp` | The single table pairing each `PieceKind`, `PieceColor` and `PieceState` with its written name, and conversions both ways. Everything that reads or writes one of them as text uses this rather than keeping its own list — see [`util/`](../util/README.md). |
| `board.hpp` | `Board` — a fixed-size grid owning the pieces on it. Add, remove, move, query by cell or by id. |

## Two decisions worth knowing

**A piece's identity is stable.** `PieceId` is assigned once and never changes,
so a piece can be followed while it moves, animated, logged and scored. Code
that identified pieces by their cell would lose track of them the moment they
started travelling — which, in this game, is most of the time.

**`has_moved` lives on the piece**, not in a side table, because exactly one
rule needs it (a pawn's double step) and keeping it elsewhere would make that
rule depend on state it cannot see.
