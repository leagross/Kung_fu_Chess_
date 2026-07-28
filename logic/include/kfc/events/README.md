# `events/` — the publish/subscribe bus

The spec asks for a bus, and this is it: the score panel, the move list, the
sound cues and the start/end animations all react to the same events without any
of them knowing the others exist.

| Header | Responsibility |
|--------|----------------|
| `event_bus.hpp` | `EventBus` — `subscribe<T>(handler)` and `publish<T>(value)`. Routing is by C++ type, not by a string name. |
| `game_events.hpp` | The whole-game signals: `GameStarted`, `GameEnded{winner}`, `OpponentCountdown{seconds}`, `OpponentReturned`. |

## Why the bus is typed

Subscribing to a type rather than a name means a misspelling is a compile error
instead of a handler that silently never fires. It also means an event carries a
real value with real fields — `GameEnded` holds the winner — rather than a
payload the subscriber has to unpack and hope about.

## What is *not* here

There is no `PieceArrived` wrapper. `kfc::model::ArrivalEvent` is published
as-is, because it already is the event. Only the whole-game signals that had no
existing type of their own are defined here.

## Threading

The bus is not internally synchronized, on purpose. Subscriptions are wired once
on the main thread before play starts, and `publish` only ever runs later on
that same thread — for a networked client, from `wait()`, which is called once
per frame. Anything crossing a thread boundary is queued before it reaches the
bus.
