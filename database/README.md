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
| `user_store` | **`IUserStore`** — what everything above sees: authenticate, read a rating, re-rate atomically. Which database is behind it is not visible from here. |
| `user_repository` | The SQLite implementation of it. Registers a username on first sight, verifies the password on every visit after that, and reads/writes ratings. Internally synchronized — the server calls it from several connection threads at once. |
| `password_hash` | Argon2id hashing and verification — what a stored credential is, and how one is checked. |
| `elo` | The rating maths and its constants: starting rating 1200, K-factor 32, ±100 matchmaking window, and the flat 10-point forfeit penalty. |
| `rating_service` | Applies a finished game's outcome to the stored ratings — the normal ELO exchange for a decisive result or a draw, the flat penalty for a disconnect. |

## How passwords are stored

**Argon2id**, with a fresh random salt per account. What is stored is one PHC
string — `$argon2id$v=19$m=19456,t=2,p=1$<salt>$<hash>` — which carries the
algorithm, the cost it was made with, and the salt, so there is no second column
to keep in step with it.

### Why not a salted SHA-256, which is what this was

SHA-256 is built to be **fast**; that is its job everywhere else. A password hash
needs the opposite. Commodity hardware computes billions of SHA-256 per second,
so a stolen table of salted SHA-256 hashes is a dictionary attack that finishes
over a weekend. The salt only forces an attacker to work one account at a time —
it never makes the work *hard*.

Argon2id makes each guess cost real time **and memory**, and the memory is the
part that matters: it is what a GPU or an ASIC cannot simply add thousands of in
parallel. The `id` variant is the default to reach for — Argon2i's resistance to
side-channel attacks on the first pass, Argon2d's resistance to time-memory
trade-offs on the rest.

Verification uses Argon2's own constant-time comparison, so how long a check
takes cannot reveal how much of a guess was right.

The cost is deliberate and visible: the test suite went from 13 to 27 seconds
when this landed. That is the feature working.

## The database file

`kfc_users.db`, created on first run in the server's working directory. It is
**not committed** (see the repository `.gitignore`): it is local state, and it
holds account data. Every machine builds its own from empty — delete it to reset
all accounts and ratings.

## Why the store is behind an interface

`Server_Design.md` concludes that SQLite cannot carry the target load — one
writer, one file, one machine — so a second implementation is a matter of when,
not whether. Naming the operations separately from the storage makes that swap a
new class and a different line in `main`, rather than an edit to every caller.

The interface is deliberately **narrow**: no `execute_sql`, no cursor, no
transaction handle. The compound operations take the arithmetic as a callback so
the implementation can hold whatever lock or transaction it needs while the
caller supplies only the decision. A `begin()`/`commit()` pair here would leak
SQLite's threading model into code that must also work against a sharded
Postgres.

`test_rating_service.cpp` builds a whole `IUserStore` out of a `std::map` in
thirty lines and runs the rating service against it, which is the proof the
abstraction is real rather than decorative.

## A result is applied in one step, not four

Every rating change is a read-modify-write, and each match has its own tick
thread, so two games can finish at the same instant and report into this one
store. Written as "read both ratings, compute, write both back", two of those
landing together lose one result entirely — the second write is computed from a
rating the first already replaced, and points appear from nowhere or vanish.

So the read, the arithmetic and the writes happen inside the store, in one
transaction, with its lock held throughout: `UserRepository::rerate_pair` and
`rerate` take the ELO calculation as a callback rather than handing the ratings
out and trusting the caller to put them back. The lock is what makes it atomic
against our own threads; the transaction is what makes the *pair* of writes
all-or-nothing against a crash — half an exchange on disk would be permanent.

## Why ELO lives here

A rating is a stored fact, so the maths that decides the next one belongs with
the store. The one constant the server also needs is
`kMatchmakingRatingGap` — the ±100 window `RoomManager` pairs players within —
and it reads it from here explicitly, which keeps the direction of the
dependency visible.

## Tests

`tests/unit/` covers the repository (registration, wrong password, rating
persistence, account isolation), the ELO curve against the spec's own worked
examples, and the rating service end to end. Each test builds a throwaway
database in a temp directory, so they never touch the real one.
