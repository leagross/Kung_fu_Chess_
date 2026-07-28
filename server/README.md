# `server/` — rooms, matches and the wire

The authoritative half of networked play. Every move is validated here through
the very same `GameEngine` local play uses, so a client cannot invent a legal
move by lying.

Depends on `logic`, `protocol` and `database`. Contains no OS-specific code —
the server builds and runs anywhere.

## The four pieces

| File | Responsibility |
|------|----------------|
| `websocket_game_server` | **How you connect.** Accepts sockets, decodes JSON, authenticates a `Login`, and routes everything after that. Knows nothing about chess. |
| `room_manager` | **Which game you are in.** Owns every live room, pairs players by rating, generates room ids, decides player vs. spectator vs. returning player, and tears down rooms once empty. |
| `match` | **What the game does.** One room's simulation: a command queue, a dedicated tick thread, and the game-over state. |
| `match_audience` | Who is attached to a match and how to reach them — the two seats and any number of watchers. All broadcasting goes through here, and none of it under a lock (see below). |
| `disconnect_watch` | The 20-second grace a dropped player gets: opening it, counting it down once per second, cancelling it on a reconnect, expiring it into a forfeit. |

## The command queue

Client threads never touch the board. They call `Match::enqueue`, and a single
tick thread per room drains that queue about 60 times a second, applies the
commands **in arrival order**, then advances the simulation.

This is what answers "who wins when two players move at the same instant?" —
whoever arrived first — deterministically, without any special case. It is also
why the single-threaded assumptions in `logic/` stay true on a multi-threaded
server.

Rooms are independent: each has its own thread, so many games run genuinely in
parallel. The queue only orders things *within* one game.

## Nothing is sent while a lock is held

`MatchAudience` keeps its roster as an immutable value behind a `shared_ptr`.
A broadcast takes a pointer to the current version under the mutex, lets go, and
*then* sends; a change in membership publishes a new version rather than editing
the one being read.

Sending under the lock would be wrong twice over. A stalled socket blocks inside
`send()` for as long as its buffers stay full, and every seating, watching and
username lookup in that match would queue behind that one dead client. And a
send or close that fails is reported back as a disconnect — which comes straight
back into the same table and deadlocks a non-recursive mutex against itself.

The cost lands where it belongs: a membership change (a few per match) copies
the roster, a broadcast (several per second, per match) copies a pointer.

## Seating

- **Play** — matchmaking. A newcomer joins the waiting room whose lone player is
  closest in rating and within ±100; if there is none, they open a room and wait.
- **Create** — a new room with a **server-generated** id: four characters from an
  alphabet that omits the pairs people misread aloud (`0/O`, `1/I/L`, `2/Z`,
  `5/S`, `8/B`). The client never picks a name, so two creators cannot collide.
- **Join** — by id. First arrival is Black; **anyone after that spectates**, and
  spectators are unlimited. Joining a room whose game is already decided is
  refused with `room_not_active` rather than dropping someone onto a dead board.

## Disconnects and returning players

A dropped player does not lose immediately. `DisconnectWatch` opens a 20-second
grace and the opponent's screen counts it down. If someone joins that room
during the countdown, the server asks whether the username matches the seat
being counted down:

- **same username** → they reclaim their own colour and position, the countdown
  is cancelled, and both sides resume;
- **anyone else** → an ordinary joiner, so a spectator. A stranger can never
  take a seat that still belongs to someone.

If the grace runs out, the opponent wins by forfeit and the dropped player takes
the flat rating penalty. A few seconds after any ending the match closes every
connection, which releases the survivor and lets the room be reaped.

## Tests

`tests/unit/test_match.cpp` covers ownership, rejections, resignation, the
countdown, forfeits and the release. `test_room_manager.cpp` covers matchmaking
windows, room ids, spectators, refusals with reasons, and telling a returning
player from a stranger. Both drive real tick threads and observe them in real
(short) wall-clock time, with recording fakes in place of sockets.
