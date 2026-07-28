# `io/` — boards as text

Reading a board from a text layout and printing one back. Used by the test
fixtures, by the server's starting position, and by the local client — all three
load the same file format.

| Header | Responsibility |
|--------|----------------|
| `text_tokenizer.hpp` | Splits a line into tokens. |
| `piece_token.hpp` | Reads one cell: `wP` is a white pawn, `bK` a black king, `.` an empty square. |
| `board_parser.hpp` | Turns a list of lines into a `Board`, assigning each piece its id. |
| `board_printer.hpp` | Turns a `Board` back into lines — the exact format the parser reads, so a round trip is lossless. |
| `parse_error.hpp` | What went wrong and on which line. |

## Why printing matters as much as parsing

The integration tests play a recorded game and compare the printed board to an
expected one. That only works because printing is the parser's exact inverse: a
board printed and re-read is the same board. It also makes a failing test
readable — the diff is two chess positions, not two object dumps.

## The format

```
. . . . . . . .
wP . . . bK . .
```

Row 0 is the top line, which is Black's back rank in the standard layout. Cells
are whitespace-separated, so a wider token (a promoted piece, a Drone) needs no
change to the format.
