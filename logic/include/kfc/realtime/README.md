# `realtime/` — the part that makes it *Kung Fu* chess

There are no turns. A move is not instant: a piece **travels**, and the board
changes only when it arrives. This directory is what that means in code, and it
is where most of the game's difficulty lives.

| Header | Responsibility |
|--------|----------------|
| `motion.hpp` | `Motion` — a piece in transit: source, destination, duration, elapsed, cooldown. |
| `motion_kind.hpp` | `MotionKind` — an ordinary move, or a jump in place. |
| `motion_factory.hpp` | Builds a `Motion`: how long this piece takes to cross this distance, and what it rests for afterwards. |
| `real_time_arbiter.hpp` | The heart. Owns every in-flight motion, advances them all by elapsed time, and produces the arrivals. |
| `collision_resolver.hpp` | What happens when two pieces arrive at the same cell in the same instant. |
| `arrival_event.hpp` | `ArrivalEvent` — a piece arrived: what moved, from where, what it captured, whether it promoted, and when. |
| `cooldown_policy.hpp`, `standard_cooldown_policy.hpp`, `jump_cooldown_policy.hpp` | How long a piece rests after moving, and after jumping. |
| `piece_speed_provider.hpp`, `piece_value_provider.hpp` | How fast each kind travels; what each is worth when captured. |
| `pawn_promotion.hpp` | Whether an arrival promotes a pawn. |
| `game_observer.hpp`, `move_log_observer.hpp`, `score_observer.hpp`, `game_over_observer.hpp` | The things that watch arrivals: the move list, the score, and whether a king fell. |

## The three ideas that matter

**A moving piece still occupies its source.** It is not removed from where it
was and added where it is going — it is *in transit*, and the board changes only
at the instant of arrival. That is what makes intercepting a moving piece, and
two pieces colliding head-on, ordinary situations rather than special cases.

**Arrivals are ordered by when they happened, not by storage order.**
`advance_time` works out each motion's arrival instant *within* the tick, sorts
them stably, and only then resolves collisions. So advancing by 1300 ms gives
exactly the same result as advancing by 1000 and then 300 — whatever tick rate
the server happens to run at cannot change who won a race.

**Time is injected, never read.** Nothing here calls a clock; callers pass the
elapsed milliseconds into `wait()`. That is why a test can play out a whole game
deterministically in microseconds, and why the server can freeze a match during
a disconnect simply by not advancing it.
