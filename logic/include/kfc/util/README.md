# `util/` — small tools with no game in them

Nothing here knows what a board is. These are general utilities that happened to
be needed more than once, kept out of the game directories so no one has to read
chess code to understand them.

| Header | What it provides |
|--------|------------------|
| `enum_names.hpp` | `EnumNames<Enum, N>` — one `constexpr` table pairing an enum's values with their written names, giving both directions of the conversion and a compile-time check that no enumerator was left out. |

## Why a table and not a pair of functions

The usual way to write an enum as text is a `switch`, and to read it back an
`if`-chain. This project had that shape five times over — for `PieceKind`,
`PieceColor`, `PieceState`, `MotionKind` and the animation states — plus a
`std::unordered_map` doing the same job for a sixth. Replacing all of them with
one table changed three things:

**It stopped allocating.** Every one of those functions returned `std::string`,
constructed from a string literal, for a name that is never modified and is only
ever compared. Encoding a full board snapshot built and destroyed three of them
per piece. The table hands out `std::string_view`s onto the literals themselves.

**It made "did you cover every value?" a build error.** `covers_through` is
written to be used in a `static_assert` next to the table. Before, adding a piece
kind and forgetting the read side compiled cleanly and failed at runtime, on a
message from a real player.

**It stopped the two directions from disagreeing.** Writing and reading were two
separate lists that had to be kept in step by hand. Now there is one list, and
the inverse is derived from it.

The lookups are linear scans. For tables of two to seven entries that is faster
than hashing — no map to build before `main()`, no pointer to chase — and it is
what keeps the whole thing usable at compile time.

## Adding one

```cpp
inline constexpr kfc::util::EnumNames<Suit, 4> kSuitNames{{{
    {Suit::Clubs, "Clubs"},
    {Suit::Diamonds, "Diamonds"},
    {Suit::Hearts, "Hearts"},
    {Suit::Spades, "Spades"},
}}};

static_assert(kSuitNames.covers_through(Suit::Spades), "every Suit needs a name");
```

Then wrap the two lookups in named free functions, so call sites read as
`name_of(suit)` and `suit_from_name(text)` rather than reaching into the table.
See `kfc/model/piece_names.hpp` for the full pattern.
