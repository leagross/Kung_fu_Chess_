#pragma once

#include <string>
#include <vector>

#include "../../kfc/model/piece.hpp"
#include "../../kfc/realtime/game_observer.hpp"

namespace kfc::model {

/// One row of the timestamped move table: when the arrival happened
/// (RealTimeArbiter's own simulated clock, the same one ArrivalEvent::
/// arrived_at_ms already carries -- not wall-clock time, and not affected
/// by how finely/coarsely the caller chunks its wait() calls) and a short
/// algebraic-style description of it.
struct MoveLogEntry {
    long long time_ms;
    std::string notation;
};

/// Keeps a human-readable move history per side, one line per arrival --
/// exactly the "score and move list, updated by an Observer, not by the
/// move-handling code itself" the graphics lecture asked for. Pure text
/// bookkeeping; formatting for a screen (an Img, a window) is a renderer's
/// job, not this class's.
class MoveLogObserver : public IGameObserver {
public:
    /// board_height is only needed to turn a Position's row into a chess
    /// rank number (rank = board_height - row) for entries()'s notation;
    /// moves() doesn't use it at all.
    explicit MoveLogObserver(int board_height = 8);

    void on_arrival(const ArrivalEvent& event) override;

    /// This side's moves so far, oldest first. Notation is
    /// "<piece token> <source>-><destination>", with " x<captured token>"
    /// appended when that arrival captured something -- e.g. "wP (6,4)->(4,4)".
    [[nodiscard]] const std::vector<std::string>& moves(PieceColor color) const;

    /// This side's moves so far, oldest first, as a timestamp plus a short
    /// algebraic-style notation (destination square, piece letter for
    /// anything but a pawn, "x" before the destination on a capture, and
    /// -- only for a pawn's capture -- the source file prefixed instead of
    /// a piece letter, e.g. "e5", "Nc6", "exd4"). This is a simplified
    /// approximation, not full SAN: no check/mate marks, no disambiguation
    /// between two same-kind pieces that could reach the same square, no
    /// en passant or castling notation (this engine doesn't implement
    /// either).
    [[nodiscard]] const std::vector<MoveLogEntry>& entries(PieceColor color) const;

private:
    int board_height_;
    std::vector<std::string> white_moves_;
    std::vector<std::string> black_moves_;
    std::vector<MoveLogEntry> white_entries_;
    std::vector<MoveLogEntry> black_entries_;
};

}  // namespace kfc::model
