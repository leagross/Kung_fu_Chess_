# `protocol/` — what client and server say to each other

Every message that crosses the socket, defined once and shared by both sides.
Because both link this same library, they cannot drift apart: a field added
here appears on both ends or neither.

Depends on `logic` (messages carry `Position`, `Piece` and `ArrivalEvent`
directly) and nlohmann/json. It knows nothing about sockets — it only turns
messages into text and back.

## The messages

**Client → server** (`ClientMessage`)

| Message | Meaning |
|---------|---------|
| `Login` | Username + password. Registers on first use, must match afterwards. |
| `Play` | Find me any rating-compatible opponent. |
| `CreateRoom` | Open a room. Carries **no name** — the server generates the id. |
| `JoinRoom` | Join the room with this id, as a player or as a spectator. |
| `MoveRequest` | Move the piece at `source` to `destination`. |
| `JumpRequest` | Jump-in-place at `cell`. |
| `Resign` | Give up; the opponent wins. |

**Server → client** (`ServerMessage`)

| Message | Meaning |
|---------|---------|
| `Welcome` | You are seated: your colour, whether you are a spectator, the room id, and the board **as it stands now** (not as it started — you may be walking in mid-game). |
| `MatchStart` | Both seats are filled; play begins. |
| `MotionStarted` | A piece just started moving, so clients can animate it immediately instead of waiting for it to arrive. |
| `BoardUpdate` | Arrivals from the last tick — the only thing that actually moves a piece. |
| `MoveRejected` | Your request was refused, with the reason. |
| `OpponentDisconnected` | Seconds left on a dropped opponent's grace period. |
| `OpponentReconnected` | They came back; clear the countdown. |
| `GameOver` | The winner, or nothing for a draw. |
| `JoinFailed` | Seating was refused: `no_such_room`, `room_not_active`, `room_name_taken`. |
| `LoginFailed` | Authentication was refused, e.g. `wrong_password`. |

## Why the failure messages exist

Without them the server could only hang up, and the client had no way to tell a
wrong password from an unreachable server from a mistyped room id — it would
wait out its own timeout and then guess. Now every refusal says which it was,
immediately, and the client shows a sentence a player can act on.

## Decoding is forgiving

Unknown message types decode to `std::nullopt` rather than throwing, and new
optional fields (`spectator`, `room`) default sensibly when absent. A client
built before a field existed still understands the message.

## Tests

`tests/unit/test_protocol_json.cpp` round-trips every message: encode, decode,
compare. `test_gameplay_config.cpp` covers reading `config/gameplay.json`, the
one file both sides load so a piece behaves identically in local and networked
play.
