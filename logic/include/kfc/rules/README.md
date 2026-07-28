# `rules/` — is this move legal?

Pure geometry and legality. Nothing here knows about time, motion or cooldowns:
a rule answers "could this piece go from A to B on this board", never "is it
allowed to right now".

| Header | Responsibility |
|--------|----------------|
| `movement_rule.hpp` | `IMovementRule` — the interface every piece's rule implements. |
| `movement_geometry.hpp` | The shared geometry: `stepping_destinations` (fixed offsets, like a knight) and `sliding_destinations` (a ray until something blocks it). |
| `king_rule.hpp` … `drone_rule.hpp` | One rule per kind: King, Queen, Rook, Bishop, Knight, Pawn, Drone. |
| `piece_rule_registry.hpp` | `IPieceRuleRegistry` — kind → rule. |
| `full_piece_rule_registry.hpp` | The registry holding all seven. |
| `rule_engine.hpp` | `RuleEngine` — asks the registry for the right rule and returns a `MoveResult`. The one entry point the engine uses. |
| `move_validation.hpp` | The checks that apply to every piece: on the board, source occupied, destination not friendly. |
| `move_reasons.hpp` | The stable strings a rejection carries (`illegal_piece_move`, `not_your_piece`, `opponent_disconnected`, …). |

## Why the geometry is shared

Rook, bishop and queen differ only in which directions they slide; king and
knight differ only in their offset list. `movement_geometry.hpp` holds that
logic once, so "stop at the first blocking piece, and capture it if it is an
enemy" is written — and fixed — in one place rather than three.

## Why rejections are strings, not an enum

A reason crosses the network to the client (`MoveRejected`). Stable strings keep
that wire contract readable in a log, and let a client show a message without
sharing a header of numeric codes.

## Why there is a class per piece

Seven small classes for seven fixed pieces is more indirection than a `switch`
would need, and that is deliberate: the assignment is about polymorphism, adding
the Drone touched no existing rule, and each rule is testable on its own.
