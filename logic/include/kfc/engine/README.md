# `engine/` — the command boundary

Where a request becomes a change. Everything outside the logic layer talks to
the game through here.

| Header | Responsibility |
|--------|----------------|
| `game_engine.hpp` | `GameEngine` — `request_move`, `request_jump`, `wait`, `is_game_over`. Validates through `RuleEngine`, starts motions through `RealTimeArbiter`, advances time. |
| `game_core.hpp` | `GameCore` — assembles the whole stack in one place: Board → RuleEngine → RealTimeArbiter → MotionFactory → GameEngine. |
| `move_requester.hpp` | `IMoveRequester` — "ask for this move". Implemented by `GameEngine` locally and by `ServerLink` over the network. |
| `move_result.hpp` | `MoveResult{is_accepted, reason}` — accepted, or refused with a stable reason. |

## Why `GameCore` exists

Local play and the server need the same stack wired the same way. Before it
existed each built its own, and the two could drift — a difference in wiring is
a difference in how the game plays. Now there is one assembly and both use it.

`GameCore` is deliberately **neither copyable nor movable.** Its members hold
references to each other (`RuleEngine` → the registry, `RealTimeArbiter` → the
board, `GameEngine` → all three), so a compiler-generated move would copy those
references verbatim and leave the new object's engine driving the *old* object's
board. That compiles, and then reads freed memory as soon as the source dies.
Deleting copy and move turns a silent use-after-move into a compile error at the
call site, which is where the mistake actually is.

Hold one by value in whatever owns it, or behind a `unique_ptr` — never return
one by value, and never put one in a container that reallocates.

## Why `IMoveRequester` exists

It is the seam that makes networked play possible without the renderer knowing.
`Controller` asks an `IMoveRequester` for a move; locally that is the engine
itself, and over the network it is a `ServerLink` that sends a message and lets
the server decide. Nothing above the seam can tell which it is talking to.
