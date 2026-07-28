# Kung Fu Chess

A real-time chess variant: there are **no turns**. Either player may move any of
their pieces at any moment, subject to a per-piece cooldown after each move.
A piece does not teleport — it *travels* to its destination over time, and the
board only changes at the instant it arrives. Capturing a king ends the game.

Built in modern **C++20** with a test-first (TDD) workflow. The engine is
headless and OpenCV-free; graphics and networking are separate layers on top.

---

## Architecture

The codebase is split into three logic layers, each in its own tree:

| Layer | Directory | Responsibility | Depends on |
|-------|-----------|----------------|------------|
| **Business logic** | `logic/` | Board, pieces, movement rules, real-time motion, game engine, text I/O. No graphics, no sockets. | — |
| **UI / graphics** | `ui/` | OpenCV-based rendering, sprite animation, mouse input, the windowed client, and the networked client (`ServerLink`). | logic |
| **Protocol** | `protocol/` | JSON message shapes + (de)serialization shared by client and server. | logic |
| **Server** | `server/` | WebSocket server hosting one match on a dedicated tick thread. | logic, protocol |

Local single-player and networked play are interchangeable behind two small
interfaces (`IGameView`, `IMoveRequester`), so the renderer never knows whether
it is driving a local `Game` or a remote `ServerLink`. Both the local `Game`
and the server's `Match` assemble the same simulation stack through a shared
`GameCore` (Board → RuleEngine → RealTimeArbiter → MotionFactory → GameEngine).

---

## Features

**Game logic**
- Board parsing from a text layout, full piece model with stable identities.
- All standard movements: King, Queen, Rook, Bishop, Knight, Pawn (plus a Drone).
- Illegal moves rejected with stable machine-readable reasons; sliding pieces
  are blocked by, and capture, the first piece in their path.
- Real-time motion: a moving piece occupies its source cell until it arrives;
  head-on collisions, promotion, and cooldowns are resolved deterministically.
- King capture ends the game (win, or a draw on a simultaneous double capture).

**Graphics**
- Transparent PNG sprites, per-piece animation with eased interpolation.
- Windowed client driven by mouse clicks; a game-over banner and a live
  score / move-list HUD.

**Networking (Phase 1)**
- WebSocket server validates every move through the same `GameEngine`.
- First two connections are assigned White / Black; the server broadcasts an
  identical game state (motion-start + arrival events) to both clients.
- Client-side motion prediction keeps animation smooth over the network.

---

## Building

Requires **CMake ≥ 3.20** and a **C++20** compiler (MSVC 2022+, GCC 10+, Clang 12+).
GoogleTest, nlohmann/json, and IXWebSocket are fetched automatically by CMake.

### Backend + tests (no OpenCV needed)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target kfc_tests
ctest --test-dir build --output-on-failure
```

### GUI + server (needs OpenCV)

The Windows GUI targets link **OpenCV 4.5.1**, expected at
`graphics/CTD26-main/OpenCV_451/` (not committed — see `.gitignore`). With it
in place:

```sh
cmake --build build --target kfc_server kfc_gui_app
```

---

## Running

### Local single-player

```sh
./build/Debug/kfc_gui_app
```

Click a piece, then click its destination. A double-click on a piece triggers
its "jump-in-place" defensive move.

### Networked (server + two clients)

Start the server (defaults to `ws://localhost:8080`):

```sh
./build/Debug/kfc_server            # optional: pass a port, e.g. 8090
```

Then launch two clients, in separate windows:

```sh
./build/Debug/kfc_gui_app --server=ws://localhost:8080 --username=player1
./build/Debug/kfc_gui_app --server=ws://localhost:8080 --username=player2
```

The first to connect plays White, the second Black. To play across two
machines, replace `localhost` with the server host's LAN IP and allow the port
through its firewall.

---

## Tests

A single `kfc_tests` target covers the logic, protocol, and server across
unit and integration suites (run via `ctest`, above). Continuous integration
builds and runs the full suite on every push (see `.github/workflows/ci.yml`).

---

## Project structure

```
logic/      business logic (model, rules, realtime, engine, io, input, texttests)
ui/         OpenCV graphics, animation, input, and the networked client
legacy/     the original CTD26 graphics demo (not built; kept for reference)
protocol/   shared JSON message shapes + (de)serialization
server/      WebSocket server and the Match that hosts one game
graphics/    asset packs (piece sprites, board, background)
docs/        generated reference / test reports
```
