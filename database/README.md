# `database/` — accounts and ratings

Everything the server remembers between sessions, and nothing else: who has an
account, whether a password is right, and what everyone is rated.

Its own `kfc::database` namespace, so the boundary is real rather than a naming
convention. This layer knows nothing about rooms, matches, sockets or the game,
and the dependency only ever points one way: **server → database**. Swapping
SQLite for something else would touch nothing outside this folder.

## What is in here

| File | Responsibility |
|------|----------------|
| `user_repository` | The store itself, in SQLite. Registers a username on first sight, verifies the password on every visit after that, and reads/writes ratings. Internally synchronized — the server calls it from several connection threads at once. |
| `sha256` | A self-contained SHA-256, so passwords are never stored in the clear and no crypto library has to be installed. |
| `elo` | The rating maths and its constants: starting rating 1200, K-factor 32, ±100 matchmaking window, and the flat 10-point forfeit penalty. |
| `rating_service` | Applies a finished game's outcome to the stored ratings — the normal ELO exchange for a decisive result or a draw, the flat penalty for a disconnect. |

## How passwords are stored

Never as text. Each account gets its own random **salt**; what is stored is
`sha256(salt + password)` alongside that salt. Verifying re-hashes the attempt
with the stored salt and compares. Two people who pick the same password get
different hashes, and the stored value cannot be read back into a password.

## The database file

`kfc_users.db`, created on first run in the server's working directory. It is
**not committed** (see the repository `.gitignore`): it is local state, and it
holds account data. Every machine builds its own from empty — delete it to reset
all accounts and ratings.

## Why ELO lives here

A rating is a stored fact, so the maths that decides the next one belongs with
the store. The one constant the server also needs is
`kMatchmakingRatingGap` — the ±100 window `RoomManager` pairs players within —
and it reads it from here explicitly, which keeps the direction of the
dependency visible.

## Tests

`tests/unit/` covers the repository (registration, wrong password, rating
persistence, account isolation), SHA-256 against known vectors, the ELO curve
against the spec's own worked examples, and the rating service end to end. Each
test builds a throwaway database in a temp directory, so they never touch the
real one.
