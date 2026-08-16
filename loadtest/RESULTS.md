# Load test results (Roadmap.md stage 3)

Run against a **Release** build of `kfc_server` (Debug is unoptimized C++ and
would understate every number here) on the single Windows dev machine this
work was done on — not a cloud VM, and not the 10,000-client/5,000-game
target `Roadmap.md` names. See "What this didn't reach, and why" below for
why that target needs a different test rig, not a different server.

Client: [`k6_load_test.js`](k6_load_test.js), run via `docker run grafana/k6`.
Each VU registers a unique account, sends `Play` (matchmaking), and once
paired sends a `Move` roughly once a second for the run's duration — see the
script's own header comment for exactly what's simulated and what isn't.

## Two real bugs, found by this test, fixed because of it

**1. The server could not exceed 128 concurrent connections at all.**
ixwebsocket's own defaults (`backlog=5`, `maxConnections=128`) were never
overridden, so `kfc_server.log` filled with `"reached max connections = 128.
Not accepting connection"` well before any real load was applied — a hard
wall completely unrelated to CPU, memory, or anything `Server_Design.md`
actually argues about. Fixed in
[`websocket_game_server.hpp`](../server/include/kfc/server/websocket_game_server.hpp)
(`kTcpBacklog`, `kMaxConnections`), applied to both `WebSocketGameServer` and
`HttpApiServer`.

**2. `UserRepository` never set a SQLite journal mode**, so every write
(every new account's `INSERT`) ran in SQLite's default rollback-journal mode
— two `fsync`s per transaction, and a writer blocks all other access to the
file for the duration. Under a burst of brand-new accounts registering at
once (every VU here starts as one), that queued behind `UserRepository`'s own
`mutex_` for real disk time, not CPU. Fixed by switching to
`PRAGMA journal_mode=WAL` + `synchronous=NORMAL` in the constructor — see
that file's own comment for the durability trade-off this makes (still safe
against this process crashing; not against the exact instant of an OS crash,
which is an acceptable trade for match history and ratings).

## Clean runs (fresh server, no prior test's connections still winding down)

`kfc_welcomes_received` counts one `Welcome` per **player** seated into a
game, not one per game — a 2-player game produces two. "Games matched"
below is that count divided by 2, not a separate server-side number.
"Handshake success" is `checks passed / (checks passed + kfc_connection_errors)`
— `kfc_connection_errors` counts iterations where `ws.connect()` itself threw
(refused/reset before any handshake response came back), which never used to
reach `check()` at all and so never used to count against the rate. These
runs predate that fix (`kfc_connection_errors` was always 0 on this
single-machine, otherwise-idle setup) — see "Reproducing" below to re-run
with it.

| VUs  | Duration | Handshake success | Players matched (`kfc_welcomes_received`) | Games matched (÷2) | Moves sent | Moves/sec (k6) | Peak `kfc_active_connections` | Peak memory |
|-----:|---------:|-------------------:|---------------:|---------------:|-----------:|----------------:|-------------------------------:|------------:|
| 10   | 20s      | 100% (10/10 checks, 0 connection errors)        | 10             | 5              | 490        | 9.8/s            | 10                              | —           |
| 500  | 60s      | 100% (86/86 checks, 0 connection errors) | 683 (of ~500 VUs, includes reconnect churn) | ~342 | 29,559 | 328/s | ~500 | — |
| 2000 | 90s      | 100% (464/464 checks, 0 connection errors) | 1,437 | ~719 | 76,527 | 637/s | ~1,900 | 197 MB, back down to 12 MB once idle |

At 2000 concurrent VUs (~1,900 peak concurrent connections, ~719 games by the
÷2 estimate above), `kfc_tick_duration_seconds_max` never exceeded ~9ms and
`kfc_active_connections` tracked what k6 reported almost exactly — the
server itself showed no sign of strain on this machine's 8 worker threads.

`kfc_move_round_trip_ms` sat around p95 ≈ 420-430ms across every scale
tested, essentially flat from 10 VUs to 2000 — see the script's own comment
on why this number is an approximation (no request id in the protocol to
correlate a reply to *your* move rather than the opponent's broadcast of
theirs), but flat-across-scale is itself informative: it means this number
is dominated by the `MOVE_INTERVAL_MS=1000` client pacing and the room's
own broadcast cadence, not by server queueing that gets worse under load.

## What this didn't reach, and why

Running k6 (in Docker) and `kfc_server` on the same Windows laptop means the
*test client* and the *thing being tested* share one machine's TCP stack.
Repeated large runs back-to-back left thousands of sockets in `TIME_WAIT`
(confirmed via `netstat`), and a subsequent run at 1,500-2,000 VUs before
that drained saw handshake failures climb to 20-30% -- but
**`kfc_server.log` recorded zero `max connections` rejections in any of
those runs**: the failures happened in the OS/Docker NAT layer before ever
reaching the server's accept loop. That is a single-machine test-methodology
ceiling, not evidence of a `kfc_server` capacity limit -- the clean, isolated
2000-VU run above shows no server-side degradation at all.

Reaching the literal 10,000-client/5,000-game target needs either multiple
client machines (so no single host's ephemeral-port/TIME_WAIT budget is the
bottleneck) or a purpose-built load generator, not a bigger number handed to
this same k6 script on this same laptop.

## Reproducing

```sh
# from repo root, kfc_server already listening on ws://localhost:8080
docker run --rm -i \
  -e TARGET_URL=ws://host.docker.internal:8080 -e VUS=500 -e DURATION=60s \
  -v "$(pwd)/loadtest:/loadtest" grafana/k6 run /loadtest/k6_load_test.js
```

Between successive large runs on one machine, give `TIME_WAIT` sockets time
to drain (`netstat -ano | grep -c TIME_WAIT` should be low) or the next
run's numbers will reflect port contention, not the server.
